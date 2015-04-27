#include "Arduino.h"
#include "LowLevel.h"
#include "SerialChannel.h"
#include "OneWireSlave.h"

#define LEDPin    13
#define OWPin 2

#ifdef ENABLE_SERIAL_CHANNEL
SerialChannel debug("debug");
#endif

Pin led(LEDPin);

byte owROM[7] = { 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

byte acknowledge = 0x42;

void owReceive(OneWireSlave::ReceiveEvent evt, byte data);

void setup()
{
	led.outputMode();
	led.writeLow();

	OneWire.setReceiveCallback(&owReceive);
	OneWire.begin(owROM, OWPin);
    
    Serial.begin(9600);
}

void loop()
{
	delay(1);

	cli();//disable interrupts
	#ifdef ENABLE_SERIAL_CHANNEL
	SerialChannel::swap();
	#endif
	sei();//enable interrupts

	#ifdef ENABLE_SERIAL_CHANNEL
	SerialChannel::flush();
	#endif
}

void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{
	switch (evt)
	{
	case OneWireSlave::RE_Byte:
		if (data == 0x01)
			led.writeHigh();
		else if (data == 0x02)
			led.writeLow();
		OneWire.write(&acknowledge, 1, 0);
		break;
	default:
		;
	}
}
