#include "Arduino.h"
#include "LowLevel.h"
#include "OneWireSlave.h"

// This is the pin that will be used for one-wire data (depending on your arduino model, you are limited to a few choices, because some pins don't have complete interrupt support)
// On Arduino Uno, you can use pin 2 or pin 3
Pin oneWireData(2);

Pin led(13);

// This is the ROM the arduino will respond to, make sure it doesn't conflict with another device
const byte owROM[7] = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

// This sample emulates a DS18B20 device (temperature sensor), so we start by defining the available commands
const byte DS18B20_START_CONVERSION = 0x44;
const byte DS18B20_READ_SCRATCHPAD = 0xBE;
const byte DS18B20_WRITE_SCRATCHPAD = 0x4E;

// TODO:
// - handle configuration (resolution, alarms)

enum DeviceState
{
	DS_WaitingReset,
	DS_WaitingCommand,
	DS_ConvertingTemperature,
	DS_TemperatureConverted,
};
volatile DeviceState state = DS_WaitingReset;

// scratchpad, with the CRC byte at the end
volatile byte scratchpad[9];

volatile unsigned long conversionStartTime = 0;

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
	delay(10);

	cli();//disable interrupts
	// Be sure to not block interrupts for too long, OneWire timing is very tight for some operations. 1 or 2 microseconds (yes, microseconds, not milliseconds) can be too much depending on your master controller, but then it's equally unlikely that you block exactly at the moment where it matters.
	// This can be mitigated by using error checking and retry in your high-level communication protocol. A good thing to do anyway.
	DeviceState localState = state;
	unsigned long localConversionStartTime = conversionStartTime;
	sei();//enable interrupts

	if (localState == DS_ConvertingTemperature && millis() > localConversionStartTime + 750)
	{
		float temperature = 42.0f; // here you could plug any logic you want to return the emulated temperature
		int16_t raw = (int16_t)(temperature * 16.0f + 0.5f);

		byte data[9];
		data[0] = (byte)raw;
		data[1] = (byte)(raw >> 8);
		for (int i = 2; i < 8; ++i)
			data[i] = 0;
		data[8] = OWSlave.crc8(data, 8);

		cli();
		memcpy((void*)scratchpad, data, 9);
		state = DS_TemperatureConverted;
		OWSlave.beginWriteBit(1, true); // now that conversion is finished, start sending ones until reset
		sei();
	}
}

void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{
	switch (evt)
	{
	case OneWireSlave::RE_Byte:
		switch (state)
		{
		case DS_WaitingCommand:
			switch (data)
			{
			case DS18B20_START_CONVERSION:
				state = DS_ConvertingTemperature;
				conversionStartTime = millis();
				OWSlave.beginWriteBit(0, true); // send zeros as long as the conversion is not finished
				break;

			case DS18B20_READ_SCRATCHPAD:
				state = DS_WaitingReset;
				OWSlave.beginWrite((const byte*)scratchpad, 9, 0);
				break;

			case DS18B20_WRITE_SCRATCHPAD:
				
				break;
			}
			break;
		}
		break;

	case OneWireSlave::RE_Reset:
		state = DS_WaitingCommand;
		break;

	case OneWireSlave::RE_Error:
		state = DS_WaitingReset;
		break;
	}
}
