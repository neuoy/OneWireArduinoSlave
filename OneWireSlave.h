#ifndef _OneWireSlave_h_
#define _OneWireSlave_h_

#include "Arduino.h"
#include "utility/LowLevel.h"

class OneWireSlave
{
public:
	enum ReceiveEvent
	{
		RE_Reset, //!< The master has sent a general reset
		RE_Byte, //!< The master just sent a byte of data
		RE_Error //!< A communication error happened (such as a timeout) ; the library will stop all 1-wire activities until the next reset
	};

	//! Starts listening for the 1-wire master, on the specified pin, as a virtual slave device identified by the specified ROM (7 bytes, starting from the family code, CRC will be computed internally). Reset, Presence, SearchRom and MatchRom are handled automatically. The library will use the external interrupt on the specified pin (note that this is usually not possible with all pins, depending on the board), as well as one hardware timer. Blocking interrupts (either by disabling them explicitely with sei/cli, or by spending time in another interrupt) can lead to malfunction of the library, due to tight timing for some 1-wire operations.
	void begin(const byte* rom, byte pinNumber);

	//! Stops all 1-wire activities, which frees hardware resources for other purposes.
	void end();

	//! Sets (or replaces) a function to be called when something is received. The callback is executed from interrupts and should be as short as possible. Failure to return quickly can prevent the library from correctly reading the next byte.
	void setReceiveCallback(void(*callback)(ReceiveEvent evt, byte data)) { clientReceiveCallback_ = callback; }

	//! Sets (or replaces) a function to be called when a bit is received. The byte reception callback is called after that if the received bit was the last of a byte. The callback is executed from interrupts and should be as short as possible. Failure to return quickly can prevent the library from correctly reading the next bit.
	void setReceiveBitCallback(void(*callback)(bool bit)) { clientReceiveBitCallback_ = callback; }

	//! Enqueues the specified bytes in the send buffer. They will be sent in the background. The optional callback is used to notify when the bytes are sent, or if an error occured. Callbacks are executed from interrupts and should be as short as possible. If bytes is null or numBytes is 0, nothing is sent, which is equivalent to calling stopWrite. In any case, calling the write function will cancel the previous write operation if it didn't complete yet.
	void write(const byte* bytes, short numBytes, void(*complete)(bool error));

	//! Sets a bit that will be sent next time the master asks for one. Optionnaly, the repeat parameter can be set to true to continue sending the same bit each time. In both cases, the send operation can be canceled by calling stopWrite.
	void writeBit(bool value, bool repeat = false, void(*bitSent)(bool error) = 0);

	//! Cancels any pending write operation, started by writeBit or write. If this function is called before the master asked for a bit, then nothing is sent to the master.
	void stopWrite();

	void alarmed(bool value);

	static byte crc8(const byte* data, short numBytes);

private:
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
	static void beginResetDetectionSendZero_();
	static void cancelResetDetection_();

	static void beginWaitReset_();
	static void beginWaitCommand_();
	static void beginReceive_();
	static void onBitReceived_(bool bit, bool error);

	static void beginSearchRom_();
	static void beginSearchRomSendBit_();
	static void continueSearchRom_(bool error);
	static void searchRomOnBitReceived_(bool bit, bool error);

	static void beginWriteBytes_(const byte* data, short numBytes, void(*complete)(bool error));
	static void beginReceiveBytes_(byte* buffer, short numBytes, void(*complete)(bool error));

	static void noOpCallback_(bool error);
	static void matchRomBytesReceived_(bool error);
	static void notifyClientByteReceived_(bool error);
	static void bitSent_(bool error);

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

	static byte searchRomBytePos_;
	static byte searchRomBitPos_;
	static bool searchRomInverse_;
	static bool resumeCommandFlag_;
	static bool alarmedFlag_;

	static const byte* sendBuffer_;
	static byte* recvBuffer_;
	static short bufferLength_;
	static short bufferPos_;
	static byte bufferBitPos_;
	static void(*receiveBytesCallback_)(bool error);
	static void(*sendBytesCallback_)(bool error);

	static bool singleBit_;
	static bool singleBitRepeat_;
	static void (*singleBitSentCallback_)(bool error);
	static void onSingleBitSent_(bool error);

	static void(*clientReceiveCallback_)(ReceiveEvent evt, byte data);
	static void(*clientReceiveBitCallback_)(bool bit);
};

extern OneWireSlave OWSlave;

#endif
