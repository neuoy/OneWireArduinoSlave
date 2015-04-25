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

#define ReadBitSamplingTime 13 // the theorical time is about 30us, but given various overhead, this is the empirical delay I've found works best (on Arduino Uno)

#define SendBitDuration 35

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

void onEnterInterrupt()
{
	//owOutTestPin.writeLow();
}

void onLeaveInterrupt()
{
	//owOutTestPin.writeHigh();
}

volatile unsigned long resetStart = (unsigned long)-1;
unsigned long lastReset = (unsigned long)-1;
unsigned long bitStart = (unsigned long)-1;
byte receivingByte = 0;
byte receivingBitPos = 0;
bool searchRomNextBit = false;
bool searchRomNextBitToSend = false;

void setup()
{
	owROM[7] = crc8((char*)owROM, 7);

	led.outputMode();
	owPin.inputMode();
	owOutTestPin.outputMode();
	owOutTestPin.writeHigh();

	led.writeLow();

	cli(); // disable interrupts
	attachInterrupt(InterruptNumber, owWaitResetInterrupt, CHANGE);

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

int count = 0;
void loop()
{
	if ((count++) % 1000 == 0)
		led.write(!led.read());
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

void owClearError()
{
	led.writeLow();
}

void owWaitResetInterrupt()
{
	onEnterInterrupt();
	bool state = owPin.read();
	unsigned long now = micros();
	if (state)
	{
		if (resetStart == (unsigned int)-1)
		{
			onLeaveInterrupt();
			return;
		}

		unsigned long resetDuration = now - resetStart;
		resetStart = (unsigned int)-1;
		if (resetDuration >= ResetMinDuration)
		{
			if (resetDuration > ResetMaxDuration)
			{
				owError("Reset too long");
				onLeaveInterrupt();
				return;
			}

			owClearError();
			lastReset = now;
			status = OS_Presence;
			setTimerEvent(PresenceWaitDuration - (micros() - now), &beginPresence);
		}
	}
	else
	{
		resetStart = now;
	}
	onLeaveInterrupt();
}

//bool debugState = false;
volatile unsigned long lastInterrupt = 0;
void owReceiveCommandInterrupt(void)
{
	onEnterInterrupt();
	unsigned long now = micros();
	if (now < lastInterrupt + 20)
	{
		onLeaveInterrupt();
		return; // don't react to our own actions
	}
	lastInterrupt = now;
	
	//debugState = !debugState;
	//owOutTestPin.write(debugState);

	//led.write(state);

	bool bit = readBit();
	/*if (bit)
		debug.SC_APPEND_STR("received bit 1");
	else
		debug.SC_APPEND_STR("received bit 0");*/

	receivingByte |= ((bit ? 1 : 0) << receivingBitPos);
	++receivingBitPos;

	if (receivingBitPos == 8)
	{
		byte receivedByte = receivingByte;
		receivingBitPos = 0;
		receivingByte = 0;
		//debug.SC_APPEND_STR_INT("received byte", (long)receivedByte);

		if (status == OS_WaitCommand && receivedByte == 0xF0)
		{
			status = OS_SearchRom;
			searchROMReadingMasterResponseBit = false;
			searchROMSendingInverse = false;
			searchROMCurrentByte = 0;
			searchROMCurrentBit = 0;
			byte currentByte = owROM[searchROMCurrentByte];
			searchRomNextBit = bitRead(currentByte, searchROMCurrentBit);
			searchRomNextBitToSend = searchROMSendingInverse ? !searchRomNextBit : searchRomNextBit;
			//attachInterrupt(InterruptNumber, onewireInterruptSearchROM, FALLING);
			setTimerEvent(10, owSearchSendBit);
			detachInterrupt(InterruptNumber);
			onLeaveInterrupt();
			return;
		}
	}

	onLeaveInterrupt();
}

bool ignoreNextFallingEdge = false;

void owSearchSendBit()
{
	onEnterInterrupt();

	// wait for a falling edge (active wait is more reliable than interrupts to send the bit fast enough)
	while (!owPin.read());
	while (owPin.read());

	//sendBit(searchRomNextBitToSend);
	if (searchRomNextBitToSend)
	{
		//delayMicroseconds(SendBitDuration);
		ignoreNextFallingEdge = false;
	}
	else
	{
		owPullLow();
		//delayMicroseconds(SendBitDuration);
		//owRelease();
		ignoreNextFallingEdge = true;
	}

	unsigned long sendBitStart = micros();

	if (searchROMSendingInverse)
	{
		searchROMSendingInverse = false;
		searchROMReadingMasterResponseBit = true;
	}
	else
	{
		searchROMSendingInverse = true;
	}

	byte currentByte = owROM[searchROMCurrentByte];
	searchRomNextBit = bitRead(currentByte, searchROMCurrentBit);
	searchRomNextBitToSend = searchROMSendingInverse ? !searchRomNextBit : searchRomNextBit;

	if (searchROMReadingMasterResponseBit)
	{
		ignoreNextFallingEdge = true;
		attachInterrupt(InterruptNumber, onewireInterruptSearchROM, FALLING);
	}
	else
	{
		setTimerEvent(10, owSearchSendBit);
	}

	delayMicroseconds(SendBitDuration - (micros() - sendBitStart));
	owRelease();

	onLeaveInterrupt();
}

void onewireInterruptSearchROM()
{
	onEnterInterrupt();

	if (ignoreNextFallingEdge)
	{
		ignoreNextFallingEdge = false;
		onLeaveInterrupt();
		return;
	}

	if (searchROMReadingMasterResponseBit)
	{
		bool bit = readBit();

		if (bit != searchRomNextBit)
		{
			debug.SC_APPEND_STR("Master didn't send our bit, leaving ROM search");
			status = OS_WaitReset;
			attachInterrupt(InterruptNumber, owWaitResetInterrupt, CHANGE);
			onLeaveInterrupt();
			return;
		}

		searchROMReadingMasterResponseBit = false;
		++searchROMCurrentBit;
		if (searchROMCurrentBit == 8)
		{
			++searchROMCurrentByte;
			searchROMCurrentBit = 0;
			//debug.SC_APPEND_STR("sent another ROM byte");
		}

		if (searchROMCurrentByte == 8)
		{
			searchROMCurrentByte = 0;
			status = OS_WaitReset;
			debug.SC_APPEND_STR("ROM sent entirely");
			attachInterrupt(InterruptNumber, owWaitResetInterrupt, CHANGE);
			onLeaveInterrupt();
			return;
		}

		byte currentByte = owROM[searchROMCurrentByte];
		searchRomNextBit = bitRead(currentByte, searchROMCurrentBit);
		searchRomNextBitToSend = searchROMSendingInverse ? !searchRomNextBit : searchRomNextBit;

		setTimerEvent(10, owSearchSendBit);
		detachInterrupt(InterruptNumber);
		onLeaveInterrupt();
		return;
	}
	else
	{
		sendBit(searchRomNextBitToSend);
		/*if (bitToSend)
			debug.SC_APPEND_STR("sent ROM search bit : 1");
		else
			debug.SC_APPEND_STR("sent ROM search bit : 0");*/

		if (searchROMSendingInverse)
		{
			searchROMSendingInverse = false;
			searchROMReadingMasterResponseBit = true;
		}
		else
		{
			searchROMSendingInverse = true;
		}
	}

	byte currentByte = owROM[searchROMCurrentByte];
	searchRomNextBit = bitRead(currentByte, searchROMCurrentBit);
	searchRomNextBitToSend = searchROMSendingInverse ? !searchRomNextBit : searchRomNextBit;

	onLeaveInterrupt();
}

bool readBit()
{
	delayMicroseconds(ReadBitSamplingTime);
	bool bit = owPin.read();
	//owOutTestPin.write(!owOutTestPin.read());
	//owOutTestPin.write(!owOutTestPin.read());
	return bit;
}

void sendBit(bool bit)
{
	if (bit)
	{
		delayMicroseconds(SendBitDuration);
		ignoreNextFallingEdge = false;
	}
	else
	{
		owPullLow();
		delayMicroseconds(SendBitDuration);
		owRelease();
		ignoreNextFallingEdge = true;
	}
}

void beginPresence()
{
	unsigned long now = micros();
	owPullLow();
	setTimerEvent(PresenceDuration, &endPresence);
	debug.SC_APPEND_STR_TIME("reset", lastReset);
	debug.SC_APPEND_STR_TIME("beginPresence", now);
}

void endPresence()
{
	unsigned long now = micros();
	owRelease();
	debug.SC_APPEND_STR_TIME("endPresence", now);

	status = OS_WaitCommand;
	attachInterrupt(InterruptNumber, owReceiveCommandInterrupt, FALLING);
	receivingByte = 0;
	receivingBitPos = 0;
	bitStart = (unsigned long)-1;
}

ISR(TIMER1_COMPA_vect) // timer1 interrupt
{
	TCCR1B = 0; // disable clock
	void(*event)() = timerEvent;
	timerEvent = 0;
	if (event != 0)
		event();
}

uint8_t crc8(char addr[], uint8_t len) {
	uint8_t crc = 0;

	while (len--) {
		uint8_t inbyte = *addr++;
		for (uint8_t i = 8; i; i--) {
			uint8_t mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}
