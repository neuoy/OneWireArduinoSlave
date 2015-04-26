#include "Arduino.h"
#include "LowLevel.h"
#include "SerialChannel.h"
#include "OneWireSlave.h"

#define LEDPin    13
#define OWPin 2

SerialChannel debug("debug");

Pin led(LEDPin);

byte owROM[7] = { 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

void setup()
{
	led.outputMode();
	led.writeLow();

	OneWire.begin(owROM, OWPin);
    
    Serial.begin(9600);
}

int count = 0;
void loop()
{
	delay(1);
	if (count++ == 1000)
	{
		led.write(!led.read());
		count = 0;
	}

	cli();//disable interrupts
	SerialChannel::swap();
	sei();//enable interrupts

	SerialChannel::flush();

	byte b;
	if (OneWire.read(b))
	{
		char msg[32];
		sprintf(msg, "Received byte : %d", (int)b);
		debug.write(msg);
	}
}
