#ifndef _SerialChannel_h_
#define _SerialChannel_h_

class SerialChannel
{
private:
    static SerialChannel* first;
    SerialChannel* next;
    
    static byte nextId;
    byte id;
    const char* name;

public:
    SerialChannel(const char* name_);
    
    void write(byte* data, short byteCount, unsigned long time = (unsigned long)-1);
    
    void write(const char* text, unsigned long time = (unsigned long)-1);
    
private:
    void handleConnection();
    void writeShort(short num);
    void writeULong(unsigned long num);
};

#endif

