#include "Arduino.h"
#include "LowLevel.h"
#include "SerialChannel.h"

#define LEDPin    13
#define OWPin 2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

#define ResetMinDuration 480
#define ResetMaxDuration 900

#define PresenceWaitDuration 30
#define PresenceDuration 300

#define BitZeroMinDuration 20
#define BitZeroMaxDuration 75

#define SendBitDuration 30

const byte InvalidBit = (byte)-1;
const byte IncompleteBit = (byte)-2;

SerialChannel debug("debug");

Pin owPin(OWPin);
Pin owOutTestPin(3);
Pin led(LEDPin);

enum OwStatus
{
	OS_WaitReset,
	OS_Presence,
	OS_AfterPresence,
	OS_WaitCommand,
	OS_SearchRom,
};
OwStatus status;

byte owROM[8] = { 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 };
byte searchROMCurrentByte = 0;
byte searchROMCurrentBit = 0;
bool searchROMSendingInverse = false;
bool searchROMReadingMasterResponseBit = false;

void owPullLow()
{
	owPin.outputMode();
	owPin.writeLow();
	owOutTestPin.writeLow();
}

void owRelease()
{
	owPin.inputMode();
	owOutTestPin.writeHigh();
}

void owWrite(bool value)
{
	if (value)
		owRelease();
	else
		owPullLow();
}

unsigned long resetStart = (unsigned long)-1;
unsigned long lastReset = (unsigned long)-1;
unsigned long bitStart = (unsigned long)-1;
byte receivingByte = 0;
byte receivingBitPos = 0;

void setup()
{
	led.outputMode();
	owPin.inputMode();
	owOutTestPin.outputMode();
	owOutTestPin.writeHigh();

	led.writeLow();

	cli(); // disable interrupts
    attachInterrupt(InterruptNumber,onewireInterrupt,CHANGE);

	// set timer0 interrupt at 250KHz (actually depends on compare match register OCR0A)
	// 4us between each tick
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0; // initialize counter value to 0
	//TCCR1B |= (1 << WGM12); // turn on CTC mode
	//TCCR1B |= (1 << CS11) | (1 << CS10); // Set 64 prescaler
	TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
	sei(); // enable interrupts
    
    Serial.begin(9600);
}

//int count = 0;
void loop()
{
	//if ((count++) % 1000 == 0)
	//	led.write(!led.read());
    cli();//disable interrupts
	SerialChannel::swap();
    sei();//enable interrupts
    
	SerialChannel::flush();
}

void(*timerEvent)() = 0;
void setTimerEvent(short microSecondsDelay, void(*event)())
{
	microSecondsDelay -= 10; // this seems to be the typical time taken to initialize the timer on Arduino Uno

	short skipTicks = (microSecondsDelay - 3) / 4; // round the micro seconds delay to a number of ticks to skip (4us per tick, so 4us must skip 0 tick, 8us must skip 1 tick, etc.)
	if (skipTicks < 1) skipTicks = 1;
	//debug.SC_APPEND_STR_INT("setTimerEvent", (long)skipTicks);
	TCNT1 = 0;
	OCR1A = skipTicks;
	timerEvent = event;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // turn on CTC mode with 64 prescaler
}

void owError(const char* message)
{
	debug.append(message);
	led.writeHigh();
	status = OS_WaitReset;
}

void owResetError()
{
	led.writeLow();
}

void onewireInterrupt(void)
{
	bool state = owPin.read();
	unsigned long now = micros();

	//led.write(state);

	if (!state)
		resetStart = now;

	// handle reset
	if (state)
	{
		unsigned long resetDuration = resetStart == (unsigned long)-1 ? (unsigned long)-1 : now - resetStart;
		resetStart = (unsigned long)-1;
		lastReset = now;

		if (resetDuration >= ResetMinDuration && resetDuration <= ResetMaxDuration)
		{
			//debug.SC_APPEND_STR_TIME("reset", now);
			owResetError();
			status = OS_Presence;
			setTimerEvent(PresenceWaitDuration - (micros() - now), &beginPresence);
			return;
		}
	}

	if (status == OS_AfterPresence)
	{
		status = OS_WaitCommand;
		receivingByte = 0;
		receivingBitPos = 0;
		bitStart = (unsigned long)-1;

		if (state)
		{
			// this is the rising edge of end-of-presence ; don't try to interpret it as a bit
			return;
		}
	}

	// read bytes
	if (status == OS_WaitCommand)
	{
		byte bit = interpretReceivedEdgeAsBit(state, now);
		if (bit != IncompleteBit)
		{
			if (bit == InvalidBit)
				return;

			receivingByte |= (bit << receivingBitPos);
			++receivingBitPos;

			if (receivingBitPos == 8)
			{
				debug.SC_APPEND_STR_INT("received byte", (long)receivingByte);

				if (status == OS_WaitCommand && receivingByte == 0xF0)
				{
					status = OS_SearchRom;
					searchROMReadingMasterResponseBit = false;
					searchROMSendingInverse = false;
					searchROMCurrentByte = 0;
					searchROMCurrentBit = 0;
					return;
				}

				receivingBitPos = 0;
				receivingByte = 0;
			}
		}
	}

	if (status == OS_SearchRom)
	{
		byte currentByte = owROM[searchROMCurrentByte];
		bool currentBit = bitRead(currentByte, searchROMCurrentBit);

		if (searchROMReadingMasterResponseBit)
		{
			byte bit = interpretReceivedEdgeAsBit(state, now);
			if (bit != IncompleteBit)
			{
				searchROMReadingMasterResponseBit = false;
				if (bit == InvalidBit)
					return;
				if ((bit == 1) != currentBit)
				{
					debug.SC_APPEND_STR_TIME("Master didn't send our bit, leaving ROM search", now);
					status = OS_WaitReset;
				}
			}
		}
		else
		{
			if (!state)
			{
				// master is pulling low, we must send our bit
				bool bitToSend = searchROMSendingInverse ? !currentBit : currentBit;
				sendBit(bitToSend);
				if (bitToSend)
					debug.SC_APPEND_STR_TIME("Sent ROM search bit : 1", now);
				else
					debug.SC_APPEND_STR_TIME("Sent ROM search bit : 0", now);

				if (searchROMSendingInverse)
				{
					searchROMSendingInverse = false;
					searchROMReadingMasterResponseBit = true;
				}
				else
				{
					searchROMSendingInverse = true;
				}

				if (searchROMCurrentBit == 8)
				{
					++searchROMCurrentByte;
					searchROMCurrentBit = 0;
					debug.SC_APPEND_STR_TIME("sent another ROM byte", now);
				}

				if (searchROMCurrentByte == 8)
				{
					searchROMCurrentByte = 0;
					status = OS_WaitReset;
					debug.SC_APPEND_STR_TIME("ROM sent entirely", now);
				}
			}
			else
			{
				debug.SC_APPEND_STR_TIME("Search ROM : seen rising edge", now);
			}
		}
	}
}

byte interpretReceivedEdgeAsBit(bool wireState, unsigned long now)
{
	if (!wireState)
	{
		// master just pulled low, this is a bit start
		//debug.SC_APPEND_STR_TIME("bit start", now);
		bitStart = now;
		return IncompleteBit;
	}
	else
	{
		if (bitStart == (unsigned long)-1)
		{
			//owError("Invalid read sequence");
			//return InvalidBit;

			// we missed the falling edge, we emulate one here (this can happen if handling of rising edge interrupt takes too long)
			bitStart = now;
		}

		// master released the line, we interpret it as a bit 1 or 0 depending on timing
		unsigned long bitLength = now - bitStart;
		bitStart = (unsigned long)-1;

		if (bitLength < BitZeroMinDuration)
		{
			// received bit = 1
			//debug.SC_APPEND_STR_TIME("received bit 1", now);
			return 1;
		}
		else if (bitLength < BitZeroMaxDuration)
		{
			// received bit = 0
			//debug.SC_APPEND_STR_TIME("received bit 0", now);
			return 0;
		}
		else
		{
			// this is not a valid bit
			owError("Invalid read timing");
			return InvalidBit;
		}
	}
}

void sendBit(bool bit)
{
	if (!bit)
	{
		owPullLow();
		delayMicroseconds(SendBitDuration);
		owRelease();
	}
}

void beginPresence()
{
	unsigned long now = micros();
	owPullLow();
	owOutTestPin.writeLow();
	setTimerEvent(PresenceDuration, &endPresence);
	debug.SC_APPEND_STR_TIME("reset", lastReset);
	debug.SC_APPEND_STR_TIME("beginPresence", now);
}

void endPresence()
{
	unsigned long now = micros();
	owRelease();
	debug.SC_APPEND_STR_TIME("endPresence", now);

	status = OS_AfterPresence;
}

ISR(TIMER1_COMPA_vect) // timer1 interrupt
{
	TCCR1B = 0; // disable clock
	void(*event)() = timerEvent;
	timerEvent = 0;
	if (event != 0)
		event();
}
