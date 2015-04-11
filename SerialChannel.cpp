#include "Arduino.h"
#include "SerialChannel.h"

short SerialChannel::nextId = 0;

SerialChannel::SerialChannel()
{
}

void SerialChannel::init(const char* name)
{
    id = nextId++;
    Serial.write((short)0);
    Serial.write(id);
    Serial.write(strlen(name));
    Serial.write(name);
}

void SerialChannel::write(byte* data, short byteCount)
{
    Serial.write(byteCount);
    Serial.write(id);
    Serial.write(data, byteCount);
}

void SerialChannel::write(const char* text)
{
    Serial.write((byte*)text, strlen(text));
}
