#include "Arduino.h"
#include "LowLevel.h"
#include "SerialChannel.h"

#define LEDPin    13
#define OWPin 2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

#define ResetMinDuration 360
#define ResetMaxDuration 900

SerialChannel debug("debug");

Pin owPin(OWPin);
Pin ledPin(LEDPin);

unsigned long resetStart = (unsigned long)-1;

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
    
    sei();//enable interrupts
    
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
		unsigned long resetDuration = now - resetStart;
		if (resetStart != (unsigned long)-1 && resetDuration >= ResetMinDuration && resetDuration <= ResetMaxDuration)
		{
			debug.write("reset");
			ledPin.writeHigh();
		}
		resetStart = (unsigned long)-1;
	}
}
