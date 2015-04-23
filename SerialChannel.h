#ifndef _SerialChannel_h_
#define _SerialChannel_h_

#define SC_APPEND_STR(str) append((byte*)str, sizeof(str)-1)
#define SC_APPEND_STR_TIME(str, time) append((byte*)str, sizeof(str)-1, time)

class SerialChannel
{
private:
	static const byte MaxPendingMessages = 16;

	struct Message
	{
		byte* data;
		unsigned long time;
		short byteCount;
		byte id;
	};

    static SerialChannel* first;
    SerialChannel* next;
    
    static byte nextId;
    byte id;
    const char* name;

	static Message buffer1[MaxPendingMessages];
	static Message buffer2[MaxPendingMessages];
	static Message* backBuffer;
	static byte backBufferPos;
	static byte frontBufferSize;

public:
    SerialChannel(const char* name_);
    
	static void beginWriteInChannel(byte id, short byteCount, unsigned long time);
    void write(byte* data, short byteCount, unsigned long time = (unsigned long)-1);
    void write(const char* text, unsigned long time = (unsigned long)-1);

	void beginWrite(short byteCount, unsigned long time = (unsigned long)-1);
	void continueWrite(byte* data, short byteCount);

	void append(byte* data, short byteCount, unsigned long time = (unsigned long)-1);
	void append(const char* text, unsigned long time = (unsigned long)-1);
	static void swap();
	static void flush();
    
private:
    static void handleConnection();
    static void writeShort(short num);
    static void writeULong(unsigned long num);
};

#endif

