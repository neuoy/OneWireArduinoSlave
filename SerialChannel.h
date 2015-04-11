#ifndef _SerialChannel_h_
#define _SerialChannel_h_

class SerialChannel
{
private:
    static short nextId;
    short id;

public:
    SerialChannel();
    
    void init(const char* name);
    
    void write(byte* data, short byteCount);
    
    void write(const char* text);
};

#endif

