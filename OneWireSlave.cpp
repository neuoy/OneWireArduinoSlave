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

OneWireSlave OneWire;

byte OneWireSlave::rom_[8];
Pin OneWireSlave::pin_;
byte OneWireSlave::tccr1bEnable_;

unsigned long OneWireSlave::resetStart_;
unsigned long OneWireSlave::lastReset_;

void(*OneWireSlave::receiveBitCallback_)(bool bit, bool error);
void(*OneWireSlave::bitSentCallback_)(bool error);

byte OneWireSlave::receivingByte_;
byte OneWireSlave::receivingBitPos_;
byte OneWireSlave::receiveTarget_;

byte OneWireSlave::searchRomBytePos_;
byte OneWireSlave::searchRomBitPos_;
bool OneWireSlave::searchRomInverse_;

ISR(TIMER1_COMPA_vect) // timer1 interrupt
{
	TCCR1B = 0; // disable clock
	void(*event)() = timerEvent;
	timerEvent = 0;
	event();
}

void OneWireSlave::begin(byte* rom, byte pinNumber)
{
	pin_ = Pin(pinNumber);
	resetStart_ = (unsigned long)-1;
	lastReset_ = 0;

	memcpy(rom_, rom, 7);
	rom_[7] = crc8_(rom_, 7);

	#ifdef DEBUG_LOG
	debug.append("Enabling 1-wire library");
	dbgOutput.outputMode();
	dbgOutput.writeHigh();
	#endif

	cli(); // disable interrupts
	pin_.inputMode();
	pin_.writeLow(); // make sure the internal pull-up resistor is disabled

	// prepare hardware timer
	TCCR1A = 0;
	TCCR1B = 0;
	TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
	tccr1bEnable_ = (1 << WGM12) | (1 << CS11) | (1 << CS10); // turn on CTC mode with 64 prescaler

	// start 1-wire activity
	beginWaitReset_();
	sei(); // enable interrupts
}

void OneWireSlave::end()
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

void OneWireSlave::beginResetDetection_()
{
	setTimerEvent_(ResetMinDuration - 50, &OneWireSlave::resetCheck_);
	resetStart_ = micros() - 50;
}

void OneWireSlave::cancelResetDetection_()
{
	disableTimer_();
	resetStart_ = (unsigned long)-1;
}

void OneWireSlave::resetCheck_()
{
	onEnterInterrupt_();
	if (!pin_.read())
	{
		pin_.attachInterrupt(&OneWireSlave::waitReset_, CHANGE);
		#ifdef DEBUG_LOG
		debug.SC_APPEND_STR("Reset detected during another operation");
		#endif
	}
	onLeaveInterrupt_();
}

void OneWireSlave::beginReceiveBit_(void(*completeCallback)(bool bit, bool error))
{
	receiveBitCallback_ = completeCallback;
	pin_.attachInterrupt(&OneWireSlave::receive_, FALLING);
}

void OneWireSlave::receive_()
{
	onEnterInterrupt_();

	pin_.detachInterrupt();
	setTimerEvent_(ReadBitSamplingTime, &OneWireSlave::readBit_);

	onLeaveInterrupt_();
}

void OneWireSlave::readBit_()
{
	onEnterInterrupt_();

	bool bit = pin_.read();
	if (bit)
		cancelResetDetection_();
	else
		beginResetDetection_();
	receiveBitCallback_(bit, false);
	//dbgOutput.writeLow();
	//dbgOutput.writeHigh();

	onLeaveInterrupt_();
}

void OneWireSlave::beginSendBit_(bool bit, void(*completeCallback)(bool error))
{
	bitSentCallback_ = completeCallback;
	if (bit)
	{
		pin_.attachInterrupt(&OneWireSlave::sendBitOne_, FALLING);
	}
	else
	{
		pin_.attachInterrupt(&OneWireSlave::sendBitZero_, FALLING);
	}
}

void OneWireSlave::sendBitOne_()
{
	onEnterInterrupt_();

	beginResetDetection_();
	bitSentCallback_(false);

	onLeaveInterrupt_();
}

void OneWireSlave::sendBitZero_()
{
	pullLow_(); // this must be executed first because the timing is very tight with some master devices

	onEnterInterrupt_();

	pin_.detachInterrupt();
	setTimerEvent_(SendBitDuration, &OneWireSlave::endSendBitZero_);

	onLeaveInterrupt_();
}

void OneWireSlave::endSendBitZero_()
{
	onEnterInterrupt_();

	releaseBus_();
	bitSentCallback_(false);

	onLeaveInterrupt_();
}

void OneWireSlave::beginWaitReset_()
{
	disableTimer_();
	pin_.attachInterrupt(&OneWireSlave::waitReset_, CHANGE);
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
			setTimerEvent_(PresenceWaitDuration - (micros() - now), &OneWireSlave::beginPresence_);
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
	setTimerEvent_(PresenceDuration, &OneWireSlave::endPresence_);
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
	receivingByte_ = 0;
	receivingBitPos_ = 0;
	beginReceiveBit_(&OneWireSlave::onBitReceived_);
}

void OneWireSlave::onBitReceived_(bool bit, bool error)
{
	if (error)
	{
		error_("Invalid bit");
		beginWaitReset_();
		return;
	}

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
				return;
			}
			else
			{
				// TODO: send command to client code
				beginWaitReset_();
				return;
			}
		}
		else
		{
			// TODO: add byte in receive buffer
			beginWaitReset_();
			return;
		}
	}
	
	beginReceiveBit_(&OneWireSlave::onBitReceived_);
}

void OneWireSlave::beginSearchRom_()
{
	searchRomBytePos_ = 0;
	searchRomBitPos_ = 0;
	searchRomInverse_ = false;

	beginSearchRomSendBit_();
}

void OneWireSlave::beginSearchRomSendBit_()
{
	byte currentByte = rom_[searchRomBytePos_];
	bool currentBit = bitRead(currentByte, searchRomBitPos_);
	bool bitToSend = searchRomInverse_ ? !currentBit : currentBit;

	beginSendBit_(bitToSend, &OneWireSlave::continueSearchRom_);
}

void OneWireSlave::continueSearchRom_(bool error)
{
	if (error)
	{
		error_("Failed to send bit");
		beginWaitReset_();
		return;
	}

	searchRomInverse_ = !searchRomInverse_;
	if (searchRomInverse_)
	{
		beginSearchRomSendBit_();
	}
	else
	{
		beginReceiveBit_(&OneWireSlave::searchRomOnBitReceived_);
	}
}

void OneWireSlave::searchRomOnBitReceived_(bool bit, bool error)
{
	if (error)
	{
		error_("Bit read error during ROM search");
		beginWaitReset_();
		return;
	}

	byte currentByte = rom_[searchRomBytePos_];
	bool currentBit = bitRead(currentByte, searchRomBitPos_);

	if (bit == currentBit)
	{
		++searchRomBitPos_;
		if (searchRomBitPos_ == 8)
		{
			searchRomBitPos_ = 0;
			++searchRomBytePos_;
		}

		if (searchRomBytePos_ == 8)
		{
			#ifdef DEBUG_LOG
			debug.SC_APPEND_STR("ROM sent entirely");
			#endif
			beginWaitReset_();
		}
		else
		{
			beginSearchRomSendBit_();
		}
	}
	else
	{
		#ifdef DEBUG_LOG
		debug.SC_APPEND_STR("Leaving ROM search");
		#endif
		beginWaitReset_();
	}
}
