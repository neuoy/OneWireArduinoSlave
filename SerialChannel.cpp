#include "Arduino.h"
#include "SerialChannel.h"

byte SerialChannel::nextId = 1;
SerialChannel* SerialChannel::first = 0;

SerialChannel::Message SerialChannel::buffer1[SerialChannel::MaxPendingMessages];
SerialChannel::Message SerialChannel::buffer2[SerialChannel::MaxPendingMessages];
SerialChannel::Message* SerialChannel::backBuffer;
byte SerialChannel::backBufferPos;
byte SerialChannel::frontBufferSize;

SerialChannel::SerialChannel(const char* name_)
    : next(0)
    , id((byte)-1)
    , name(name_)
{
    if(first == 0)
        first = this;
    else
    {
        SerialChannel* c = first;
        while(c->next != 0) c = c->next;
        c->next = this;
    }
    
    id = nextId++;
}

void SerialChannel::beginWriteInChannel(byte id, short byteCount, unsigned long time)
{
	Serial.write("START");
	Serial.write(id);
	writeULong(time);
	writeShort(byteCount);
}

void SerialChannel::write(byte* data, short byteCount, unsigned long time)
{
	beginWrite(byteCount, time);
	continueWrite(data, byteCount);
}

void SerialChannel::beginWrite(short byteCount, unsigned long time)
{
	if (time == (unsigned long)-1)
		time = micros();

	handleConnection();

	beginWriteInChannel(id, byteCount, time);
}

void SerialChannel::continueWrite(byte* data, short byteCount)
{
	Serial.write(data, byteCount);
}

void SerialChannel::write(const char* text, unsigned long time)
{
    write((byte*)text, strlen(text), time);
}

void SerialChannel::append(byte* data, short byteCount, unsigned long time)
{
	if (time == (unsigned long)-1)
		time = micros();

	Message& msg = backBuffer[backBufferPos++];
	if (backBufferPos >= MaxPendingMessages)
		backBufferPos = MaxPendingMessages - 1;
	msg.id = id;
	msg.data = data;
	msg.byteCount = byteCount;
	msg.time = time;
}

void SerialChannel::append(const char* text, unsigned long time)
{
	append((byte*)text, strlen(text), time);
}

void SerialChannel::swap()
{
	backBuffer = backBuffer == buffer1 ? buffer2 : buffer1;
	frontBufferSize = backBufferPos;
	backBufferPos = 0;
}

void SerialChannel::flush()
{
	handleConnection();

	Message* frontBuffer = backBuffer == buffer1 ? buffer2 : buffer1;
	for (Message* msg = frontBuffer; msg < frontBuffer + frontBufferSize; ++msg)
	{
		beginWriteInChannel(msg->id, msg->byteCount, msg->time);
		Serial.write(msg->data, msg->byteCount);
	}
}

void SerialChannel::writeShort(short num)
{
    Serial.write((byte*)&num, 2);
}

void SerialChannel::writeULong(unsigned long num)
{
    Serial.write((byte*)&num, 4);
}

void SerialChannel::handleConnection()
{
    int b = Serial.read();
    if(b == (int)'C')
    {
        Serial.write("CONNECTION");
        SerialChannel* c = first;
        while(c)
        {
            Serial.write("START");
            Serial.write(0);
            Serial.write("ChannelInit");
            Serial.write(c->id);
            writeShort(strlen(c->name));
            Serial.write(c->name);
            c = c->next;
        }
		Serial.flush();
    }
}

