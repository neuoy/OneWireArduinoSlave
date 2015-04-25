#include "Arduino.h"
#include "LowLevel.h"
#include "SerialChannel.h"
#include "OneWireSlave.h"

#define LEDPin    13
#define OWPin 2

SerialChannel debug("debug");

Pin led(LEDPin);

byte owROM[7] = { 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };
OneWireSlave oneWire(owROM, OWPin);

void setup()
{
	led.outputMode();
	
	led.writeLow();

	oneWire.enable();
    
    Serial.begin(9600);
}

//int count = 0;
void loop()
{
	/*if (count++ == 10000)
	{
		led.write(!led.read());
		count = 0;
	}*/

	cli();//disable interrupts
	SerialChannel::swap();
	sei();//enable interrupts

	SerialChannel::flush();

	byte b;
	if (oneWire.read(b))
	{
		char msg[32];
		sprintf(msg, "Received byte : %d", (int)b);
		debug.write(msg);
	}
}
