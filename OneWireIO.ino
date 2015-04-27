#include "Arduino.h"
#include "LowLevel.h"
#include "SerialChannel.h"
#include "OneWireSlave.h"

#define LEDPin    13
#define OWPin 2

SerialChannel debug("debug");

Pin led(LEDPin);

byte owROM[7] = { 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

byte buffer1[8];
byte buffer2[8];
volatile byte* backBuffer = buffer1;
volatile byte bufferPos = 0;

byte sendBuffer[8];

void owReceive(OneWireSlave::ReceiveEvent evt, byte data);

void setup()
{
	led.outputMode();
	led.writeLow();

	OneWire.setReceiveCallback(&owReceive);
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
	byte* frontBuffer = (byte*)backBuffer;
	byte frontBufferSize = bufferPos;
	backBuffer = backBuffer == buffer1 ? buffer2 : buffer1;
	bufferPos = 0;
	sei();//enable interrupts

	SerialChannel::flush();

	for (int i = 0; i < frontBufferSize; ++i)
	{
		char msg[16];
		sprintf(msg, "Received byte: %d", (int)frontBuffer[i]);
		debug.write(msg);

		if (frontBuffer[i] == 0x42)
		{
			sendBuffer[0] = 0xBA;
			sendBuffer[1] = 0xAD;
			sendBuffer[2] = 0xF0;
			sendBuffer[3] = 0x0D;
			OneWire.write(sendBuffer, 4, 0);
		}
	}
}

void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{
	switch (evt)
	{
	case OneWireSlave::RE_Byte:
		backBuffer[bufferPos++] = data;
		break;
	default:
		;
	}
}
