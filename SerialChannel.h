#ifndef _SerialChannel_h_
#define _SerialChannel_h_

//#define ENABLE_SERIAL_CHANNEL

#ifdef ENABLE_SERIAL_CHANNEL
#define SC_APPEND_STR(str) append((byte*)str, sizeof(str)-1)
#define SC_APPEND_STR_INT(str, arg0) appendInt(str, sizeof(str)-1, arg0)

#define SC_APPEND_STR_TIME(str, time) append((byte*)str, sizeof(str)-1, time)

class SerialChannel
{
public:
	struct Message
	{
		unsigned long time;
		long longArg0;
		byte* data;
		short byteCount;
		byte id;
	};

private:
	static const byte MaxPendingMessages = 32;

    static SerialChannel* first;
    SerialChannel* next;
    
    static byte nextId;
    byte id;
    const char* name;

	static Message buffer1[MaxPendingMessages];
	static Message buffer2[MaxPendingMessages];
	static volatile Message* backBuffer;
	static volatile byte backBufferPos;
	static byte frontBufferSize;

public:
    SerialChannel(const char* name_);
    
	static void beginWriteInChannel(byte id, short byteCount, unsigned long time);
    void write(byte* data, short byteCount, unsigned long time = (unsigned long)-1);
    void write(const char* text, unsigned long time = (unsigned long)-1);

	void beginWrite(short byteCount, unsigned long time = (unsigned long)-1);
	void continueWrite(byte* data, short byteCount);

	Message& append(byte* data, short byteCount, unsigned long time = (unsigned long)-1);
	void append(const char* text, unsigned long time = (unsigned long)-1);
	void appendInt(const char* text, short textLength, int arg0, unsigned long time = (unsigned long)-1);
	static void swap();
	static void flush();
    
private:
    static void handleConnection();
    static void writeShort(short num);
    static void writeULong(unsigned long num);
};
#endif // ENABLE_SERIAL_CHANNEL

#endif

