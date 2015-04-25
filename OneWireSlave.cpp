#include "OneWireSlave.h"

#define DEBUG_LOG

#ifdef DEBUG_LOG
#include "SerialChannel.h"
extern SerialChannel debug;
Pin dbgOutput(3);
#endif

namespace
{
	const unsigned long ResetMinDuration = 480;
	const unsigned long ResetMaxDuration = 900;

	const unsigned long PresenceWaitDuration = 30;
	const unsigned long PresenceDuration = 300;

	const unsigned long ReadBitSamplingTime = 30;

	const unsigned long SendBitDuration = 35;

	const byte ReceiveCommand = (byte)-1;

	void(*timerEvent)() = 0;
}

OneWireSlave* OneWireSlave::inst_ = 0;

ISR(TIMER1_COMPA_vect) // timer1 interrupt
{
	TCCR1B = 0; // disable clock
	void(*event)() = timerEvent;
	timerEvent = 0;
	event();
}

OneWireSlave::OneWireSlave(byte* rom, byte pinNumber)
	: pin_(pinNumber)
	, resetStart_((unsigned long)-1)
	, lastReset_(0)
	, ignoreNextEdge_(false)
{
	inst_ = this; // we can have only one instance in the current implementation
	memcpy(rom_, rom, 7);
	rom_[7] = crc8_(rom_, 7);
}

void OneWireSlave::enable()
{
	#ifdef DEBUG_LOG
	debug.append("Enabling 1-wire library");
	dbgOutput.outputMode();
	dbgOutput.writeHigh();
	#endif


	cli(); // disable interrupts
	pin_.inputMode();
	// prepare hardware timer
	TCCR1A = 0;
	TCCR1B = 0;
	TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
	tccr1bEnable_ = (1 << WGM12) | (1 << CS11) | (1 << CS10); // turn on CTC mode with 64 prescaler
	beginWaitReset_();
	sei(); // enable interrupts
}

void OneWireSlave::disable()
{
	#ifdef DEBUG_LOG
	debug.append("Disabling 1-wire library");
	#endif

	cli();
	disableTimer_();
	pin_.detachInterrupt();
	releaseBus_();
	sei();
}

bool OneWireSlave::read(byte& b)
{
	return false;
}

void OneWireSlave::setReceiveCallback(void(*callback)())
{

}

void OneWireSlave::write(byte* bytes, short numBytes, void(*complete)(bool error))
{

}

byte OneWireSlave::crc8_(byte* data, short numBytes)
{
	byte crc = 0;

	while (numBytes--) {
		byte inbyte = *data++;
		for (byte i = 8; i; i--) {
			byte mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}

void OneWireSlave::setTimerEvent_(short delayMicroSeconds, void(*handler)())
{
	delayMicroSeconds -= 10; // remove overhead (tuned on Arduino Uno)

	short skipTicks = (delayMicroSeconds - 3) / 4; // round the micro seconds delay to a number of ticks to skip (4us per tick, so 4us must skip 0 tick, 8us must skip 1 tick, etc.)
	if (skipTicks < 1) skipTicks = 1;
	TCNT1 = 0;
	OCR1A = skipTicks;
	timerEvent = handler;
	TCCR1B = tccr1bEnable_;
}

void OneWireSlave::disableTimer_()
{
	TCCR1B = 0;
}

void OneWireSlave::onEnterInterrupt_()
{
	dbgOutput.writeLow();
}

void OneWireSlave::onLeaveInterrupt_()
{
	dbgOutput.writeHigh();
}

void OneWireSlave::error_(const char* message)
{
#ifdef DEBUG_LOG
	debug.append(message);
#endif
	beginWaitReset_();
}

void OneWireSlave::pullLow_()
{
	pin_.outputMode();
	pin_.writeLow();
#ifdef DEBUG_LOG
	//dbgOutput.writeLow();
#endif
}

void OneWireSlave::releaseBus_()
{
	pin_.inputMode();
#ifdef DEBUG_LOG
	//dbgOutput.writeHigh();
#endif
}

void OneWireSlave::beginWaitReset_()
{
	disableTimer_();
	pin_.attachInterrupt(&OneWireSlave::waitResetHandler_, CHANGE);
	resetStart_ = (unsigned int)-1;
}

void OneWireSlave::waitReset_()
{
	onEnterInterrupt_();
	bool state = pin_.read();
	unsigned long now = micros();
	if (state)
	{
		if (resetStart_ == (unsigned int)-1)
		{
			onLeaveInterrupt_();
			return;
		}

		unsigned long resetDuration = now - resetStart_;
		resetStart_ = (unsigned int)-1;
		if (resetDuration >= ResetMinDuration)
		{
			if (resetDuration > ResetMaxDuration)
			{
				error_("Reset too long");
				onLeaveInterrupt_();
				return;
			}

			lastReset_ = now;
			pin_.detachInterrupt();
			setTimerEvent_(PresenceWaitDuration - (micros() - now), &OneWireSlave::beginPresenceHandler_);
		}
	}
	else
	{
		resetStart_ = now;
	}
	onLeaveInterrupt_();
}

void OneWireSlave::beginPresence_()
{
	unsigned long now = micros();
	pullLow_();
	setTimerEvent_(PresenceDuration, &OneWireSlave::endPresenceHandler_);
	#ifdef DEBUG_LOG
	debug.SC_APPEND_STR_TIME("reset", lastReset_);
	debug.SC_APPEND_STR_TIME("beginPresence", now);
	#endif
}

void OneWireSlave::endPresence_()
{
	unsigned long now = micros();
	releaseBus_();
	#ifdef DEBUG_LOG
	debug.SC_APPEND_STR_TIME("endPresence", now);
	#endif

	beginWaitCommand_();
}

void OneWireSlave::beginWaitCommand_()
{
	receiveTarget_ = ReceiveCommand;
	beginReceive_();
}

void OneWireSlave::beginReceive_()
{
	ignoreNextEdge_ = true;
	pin_.attachInterrupt(&OneWireSlave::receiveHandler_, FALLING);
	receivingByte_ = 0;
	receivingBitPos_ = 0;
}

void OneWireSlave::receive_()
{
	onEnterInterrupt_();

	if (!ignoreNextEdge_)
		setTimerEvent_(ReadBitSamplingTime, &OneWireSlave::receiveBitHandler_);

	ignoreNextEdge_ = false;

	onLeaveInterrupt_();
}

void OneWireSlave::receiveBit_()
{
	onEnterInterrupt_();

	bool bit = pin_.read();
	//dbgOutput.writeLow();
	//dbgOutput.writeHigh();

	receivingByte_ |= ((bit ? 1 : 0) << receivingBitPos_);
	++receivingBitPos_;

	if (receivingBitPos_ == 8)
	{
		#ifdef DEBUG_LOG
		debug.SC_APPEND_STR_INT("received byte", (long)receivingByte_);
		#endif
		if (receiveTarget_ == ReceiveCommand)
		{
			if (receivingByte_ == 0xF0)
			{
				beginSearchRom_();
			}
			else
			{
				// TODO: send command to client code
				beginWaitReset_();
			}
		}
		else
		{
			// TODO: add byte in receive buffer
			beginWaitReset_();
		}
	}

	onLeaveInterrupt_();
}

void OneWireSlave::beginSearchRom_()
{
	pin_.detachInterrupt();
}