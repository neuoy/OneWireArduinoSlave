#ifndef _OneWireSlave_h_
#define _OneWireSlave_h_

#include "Arduino.h"

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
};

#endif
