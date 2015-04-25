#ifndef _OneWireSlave_h_
#define _OneWireSlave_h_

#include "Arduino.h"
#include "LowLevel.h"

class OneWireSlave
{
public:
	///! Constructs a 1-wire slave that will be identified by the specified ROM (7 bytes, starting from the family code, CRC will be computed internally). Call enable to actually start listening for the 1-wire master.
	OneWireSlave(byte* rom, byte pinNumber);

	///! Starts listening for the 1-wire master. Reset, Presence and SearchRom are handled automatically. The library will use interrupts on the pin specified in the constructor, as well as one hardware timer. Blocking interrupts (either by disabling them explicitely with sei/cli, or by spending time in another interrupt) can lead to malfunction of the library, due to tight timing for some 1-wire operations.
	void enable();

	///! Stops all 1-wire activities, which frees hardware resources for other purposes.
	void disable();

	///! Pops one byte from the receive buffer, or returns false if no byte has been received.
	bool read(byte& b);

	///! Sets (or replaces) a function to be called when at least one byte has been received. Callbacks are executed from interrupts and should be as short as possible.
	void setReceiveCallback(void(*callback)());

	///! Enqueues the specified bytes in the send buffer. They will be sent in the background. The optional callback is used to notify when the bytes are sent, or if an error occured. Callbacks are executed from interrupts and should be as short as possible.
	void write(byte* bytes, short numBytes, void(*complete)(bool error));

private:
	byte crc8_(byte* data, short numBytes);

	void setTimerEvent_(short delayMicroSeconds, void(*handler)());
	void disableTimer_();

	void onEnterInterrupt_();
	void onLeaveInterrupt_();

	void error_(const char* message);

	void pullLow_();
	void releaseBus_();

	void beginWaitReset_();
	void beginWaitCommand_();

	// interrupt handlers
	inline static void waitResetHandler_() { inst_->waitReset_(); }
	void waitReset_();
	inline static void beginPresenceHandler_() { inst_->beginPresence_(); }
	void beginPresence_();
	inline static void endPresenceHandler_() { inst_->endPresence_(); }
	void endPresence_();

private:
	static OneWireSlave* inst_;
	byte rom_[8];
	Pin pin_;
	byte tccr1bEnable_;

	unsigned long resetStart_;
	unsigned long lastReset_;
};

#endif
