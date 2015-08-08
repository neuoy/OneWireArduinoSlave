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
byte commandCheckError = 0xF1;

int turnOnTimeoutSeconds = 5 * 60;

const byte CMD_TurnOn = 0x01;
const byte CMD_TurnOff = 0x02;
const byte CMD_ReadState = 0x03;

const byte ANS_StateIsOn = 0x01;
const byte ANS_StateIsOff = 0x02;

volatile byte command = 0x0;
volatile long turnOnStartTime = 0;

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

	long now = millis();
	if (now > turnOnStartTime + (long)turnOnTimeoutSeconds * 1000 || now < turnOnStartTime)
	{
		led.writeLow();
		turnOnStartTime = 0;
	}

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
		if (command == 0x00)
		{
			command = data;
		}
		else
		{
			if (data == (0xFF-command)) // to check there is no communication error, the master must send the command, and then its inverse
			{
				byte receivedCommand = command;
				command = 0x0;
				switch (receivedCommand)
				{
				case CMD_TurnOn:
					turnOnStartTime = millis();
					led.writeHigh();
					OneWire.write(&acknowledge, 1, NULL);
					break;
				case CMD_TurnOff:
					led.writeLow();
					turnOnStartTime = 0;
					OneWire.write(&acknowledge, 1, NULL);
					break;
				case CMD_ReadState:
					bool state = led.read();
					static byte response[2];
					response[0] = state ? ANS_StateIsOn : ANS_StateIsOff;
					response[1] = 0xFF - response[0];
					OneWire.write(response, 2, NULL);
					break;
				}
			}
			else
			{
				command = 0x0;
				OneWire.write(&commandCheckError, 1, NULL);
			}
		}
		break;
	case OneWireSlave::RE_Reset:
	case OneWireSlave::RE_Error:
		command = 0x0;
		break;
	default:
		;
	}
}
