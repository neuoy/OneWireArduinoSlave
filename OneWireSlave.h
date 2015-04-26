#ifndef _OneWireSlave_h_
#define _OneWireSlave_h_

#include "Arduino.h"
#include "LowLevel.h"

class OneWireSlave
{
public:
	//! Starts listening for the 1-wire master, on the specified pin, as a virtual slave device identified by the specified ROM (7 bytes, starting from the family code, CRC will be computed internally). Reset, Presence, SearchRom and MatchRom are handled automatically. The library will use the external interrupt on the specified pin (note that this is usually not possible with all pins, depending on the board), as well as one hardware timer. Blocking interrupts (either by disabling them explicitely with sei/cli, or by spending time in another interrupt) can lead to malfunction of the library, due to tight timing for some 1-wire operations.
	void begin(byte* rom, byte pinNumber);

	//! Stops all 1-wire activities, which frees hardware resources for other purposes.
	void end();

	//! Pops one byte from the receive buffer, or returns false if no byte has been received.
	bool read(byte& b);

	//! Sets (or replaces) a function to be called when at least one byte has been received. Callbacks are executed from interrupts and should be as short as possible.
	void setReceiveCallback(void(*callback)());

	//! Enqueues the specified bytes in the send buffer. They will be sent in the background. The optional callback is used to notify when the bytes are sent, or if an error occured. Callbacks are executed from interrupts and should be as short as possible.
	void write(byte* bytes, short numBytes, void(*complete)(bool error));

private:
	static byte crc8_(byte* data, short numBytes);

	static void setTimerEvent_(short delayMicroSeconds, void(*handler)());
	static void disableTimer_();

	static void onEnterInterrupt_();
	static void onLeaveInterrupt_();

	static void error_(const char* message);

	static void pullLow_();
	static void releaseBus_();

	static void beginReceiveBit_(void(*completeCallback)(bool bit, bool error));
	static void beginSendBit_(bool bit, void(*completeCallback)(bool error));

	static void beginResetDetection_();
	static void cancelResetDetection_();

	static void beginWaitReset_();
	static void beginWaitCommand_();
	static void beginReceive_();
	static void onBitReceived_(bool bit, bool error);

	static void beginSearchRom_();
	static void beginSearchRomSendBit_();
	static void continueSearchRom_(bool error);
	static void searchRomOnBitReceived_(bool bit, bool error);

	static void beginWriteBytes_(byte* data, short numBytes, void(*complete)(bool error));
	static void beginReceiveBytes_(byte* buffer, short numBytes, void(*complete)(bool error));

	static void noOpCallback_(bool error);
	static void matchRomBytesReceived_(bool error);
	static void notifyClientByteReceived_(bool error);

	// interrupt handlers
	static void waitReset_();
	static void beginPresence_();
	static void endPresence_();
	static void receive_();
	static void readBit_();
	static void sendBitOne_();
	static void sendBitZero_();
	static void endSendBitZero_();
	static void resetCheck_();

private:
	static byte rom_[8];
	static byte scratchpad_[8];
	static Pin pin_;
	static byte tccr1bEnable_;

	static unsigned long resetStart_;
	static unsigned long lastReset_;

	static void(*receiveBitCallback_)(bool bit, bool error);
	static void(*bitSentCallback_)(bool error);

	static byte receivingByte_;
	static byte receivingBitPos_;
	static byte receiveTarget_;

	static byte searchRomBytePos_;
	static byte searchRomBitPos_;
	static bool searchRomInverse_;

	static byte* receiveBytesBuffer_;
	static short receiveBytesLength_;
	static void(*receiveBytesCallback_)(bool error);
};

extern OneWireSlave OneWire;

#endif
