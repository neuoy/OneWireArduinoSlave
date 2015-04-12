#include "Arduino.h"
#include "SerialChannel.h"

byte SerialChannel::nextId = 1;
SerialChannel* SerialChannel::first = 0;

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

void SerialChannel::write(byte* data, short byteCount, unsigned long time)
{
    if (time == (unsigned long)-1)
        time = micros();
        
    handleConnection();

    Serial.write("START");
    Serial.write(id);
    writeULong(time);
    writeShort(byteCount);
    Serial.write(data, byteCount);
}

void SerialChannel::write(const char* text, unsigned long time)
{
    write((byte*)text, strlen(text), time);
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
    }
}

