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

SerialChannel debug("debug");

Pin owPin(OWPin);
Pin owOutTestPin(3);
Pin led(LEDPin);

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
    
    Serial.begin(400000);
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

void onewireInterrupt(void)
{
	bool state = owPin.read();
	unsigned long now = micros();

	//led.write(state);

	if (!state)
		resetStart = now;

	if (state)
	{
		unsigned long resetDuration = resetStart == (unsigned long)-1 ? (unsigned long)-1 : now - resetStart;
		resetStart = (unsigned long)-1;

		if (resetDuration >= ResetMinDuration && resetDuration <= ResetMaxDuration)
		{
			//debug.SC_APPEND_STR_TIME("reset", now);
			setTimerEvent(PresenceWaitDuration - (micros() - now), &beginPresence);
			return;
		}
	}
}

void beginPresence()
{
	//debug.SC_APPEND_STR("beginPresence");
	owPullLow();
	owOutTestPin.writeLow();
	setTimerEvent(PresenceDuration, &endPresence);
}

void endPresence()
{
	//debug.SC_APPEND_STR("endPresence");
	owRelease();
}

ISR(TIMER1_COMPA_vect) // timer1 interrupt
{
	TCCR1B = 0; // disable clock
	void(*event)() = timerEvent;
	timerEvent = 0;
	if (event != 0)
		event();
}
