#include "Arduino.h"
#include "LowLevel.h"
#include "SerialChannel.h"

#define LEDPin    13
#define OWPin 2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

#define ResetMinDuration 480
#define ResetMaxDuration 900

#define PresenceWaitDuration 30
#define PresenceDuration 150

SerialChannel debug("debug");

Pin owPin(OWPin);
Pin ledPin(LEDPin);

unsigned long resetStart = (unsigned long)-1;

enum Status
{
	S_WaitReset,
	S_WaitPresence,
	S_Presence,
};

Status status = S_WaitReset;
unsigned long statusChangeTime = (unsigned long)-1;

void setup()
{
	ledPin.outputMode();
	owPin.inputMode();

	ledPin.writeLow();

    attachInterrupt(InterruptNumber,onewireInterrupt,CHANGE);
    
    Serial.begin(400000);
}

void loop()
{
    cli();//disable interrupts
	SerialChannel::swap();
    sei();//enable interrupts
    
	SerialChannel::flush();
}

void onewireInterrupt(void)
{
	bool state = owPin.read();
	unsigned long now = micros();

	//ledPin.write(state);

	if (!state)
		resetStart = now;

	if (state)
	{
		unsigned long resetDuration = resetStart == (unsigned long)-1 ? (unsigned long)-1 : now - resetStart;
		resetStart = (unsigned long)-1;

		if (resetDuration >= ResetMinDuration && resetDuration <= ResetMaxDuration)
		{
			debug.SC_APPEND_STR_TIME("reset", now);
			status = S_WaitPresence;
			statusChangeTime = now;
			return;
		}
	}
}
