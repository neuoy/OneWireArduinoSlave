#include "Arduino.h"
#include "LowLevel.h"
#include "OneWireSlave.h"

// This is the pin that will be used for one-wire data (depending on your arduino model, you are limited to a few choices, because some pins don't have complete interrupt support)
// On Arduino Uno, you can use pin 2 or pin 3
Pin oneWireData(2);

Pin led(13);

// This is the ROM the arduino will respond to, make sure it doesn't conflict with another device
const byte owROM[7] = { 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

const byte acknowledge = 0x42;

// This sample implements a simple protocol : sending match ROM, then the ROM, then 0x01 will turn the arduino light on. Sending 0x02 will turn it off. In each case, the byte 0x42 is sent as acknowledgement.
const byte CMD_TurnOn = 0x01;
const byte CMD_TurnOff = 0x02;

// This function will be called each time the OneWire library has an event to notify (reset, error, byte received)
void owReceive(OneWireSlave::ReceiveEvent evt, byte data);

void setup()
{
	led.outputMode();
	led.writeLow();

	// Setup the OneWire library
	OWSlave.setReceiveCallback(&owReceive);
	OWSlave.begin(owROM, oneWireData.getPinNumber());
}

void loop()
{
	delay(1);

	// You can do anything you want here, the OneWire library works entirely in background, using interrupts.

	cli();//disable interrupts
	// Be sure to not block interrupts for too long, OneWire timing is very tight for some operations. 1 or 2 microseconds (yes, microseconds, not milliseconds) can be too much depending on your master controller, but then it's equally unlikely that you block exactly at the moment where it matters.
	// This can be mitigated by using error checking and retry in your high-level communication protocol. A good thing to do anyway.
	sei();//enable interrupts
}

void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{
	switch (evt)
	{
	case OneWireSlave::RE_Byte:
		if (data == CMD_TurnOn)
		{
			led.writeHigh();
		}
		else if (data == CMD_TurnOff)
		{
			led.writeLow();
		}
		else
		{
			break;
		}

		// in this simple example we just reply with one byte to say we've processed the command
		// a real application should have a CRC system to ensure messages are not corrupt, for both directions
		// you can use the static OneWireSlave::crc8 method to add CRC checks in your communication protocol (it conforms to standard one-wire CRC checks, that is used to compute the ROM last byte for example)
		OWSlave.write(&acknowledge, 1, NULL);

		break;
	
	default:
		; // we could also react to reset and error notifications, but not in this sample
	}
}
