#include "Arduino.h"
#include "SerialChannel.h"

#define LEDPin    13
#define OWPin     2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

SerialChannel debug("debug");

void setup()
{
    pinMode(LEDPin, OUTPUT); 
    pinMode(OWPin, INPUT);

    digitalWrite(LEDPin, LOW);

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
    digitalWrite(LEDPin, digitalRead(OWPin));
}
