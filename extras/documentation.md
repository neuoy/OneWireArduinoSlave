# OneWireSlave library documentation

This library allows you to emulate existing 1-wire devices with an Arduino, or to create your own protocol. All low-level details are handled by the library, such as reset detection, ROM matching, byte sending and receiving. Look at the demo sketch to see an example.

## Compatible boards

If you have tested the library on another board, please send a pull-request (or just tell me which board) to update this list.

### ATmega328

- Arduino Uno (tested by [neuoy](https://github.com/neuoy))

## Getting started

See example "FakeDS18B20", and the associated [documentation](examples/FakeDS18B20/README.md), for a working use case of the library.

## Library reference

### setReceiveCallback

`void setReceiveCallback(void(*callback)(ReceiveEvent evt, byte data))`

Sets the function that will be called back when an event occurs, such as when a byte is received.

### begin

`void begin(byte* rom, byte pinNumber)`

Starts the library, which will respond to the provided ROM (must be unique on your network) on the specified Arduino pin. The ROM is cloned by the library, so you can discard your buffer immediately if you want.

### write
`bool write(const byte* bytes, short numBytes)`

Writes the specified bytes synchronously. This function blocks until the write operation has finished. Do not call from an interrupt handler! Returns true in case of success, false if an error occurs.

### beginWrite
`void beginWrite(const byte* bytes, short numBytes, void(*complete)(bool error))`

Starts sending the specified bytes. They will be sent in the background, and the buffer must remain valid and unchanged until the write operation has finished or is cancelled. The optional callback is used to notify when the bytes are sent, or if an error occured. Callbacks are executed from interrupts and should be as short as possible. If `bytes` is null or `numBytes` is 0, nothing is sent, which is equivalent to calling `stopWrite`. In any case, calling the write function will cancel the previous write operation if it didn't complete yet.

### setReceiveBitCallback
`void setReceiveBitCallback(void(*callback)(bool bit))`

Sets (or replaces) a function to be called when a bit is received. The byte reception callback is called after that if the received bit was the last of a byte. The callback is executed from interrupts and should be as short as possible. Failure to return quickly can prevent the library from correctly reading the next bit.

### writeBit
`bool writeBit(bool value)`

Writes a single bit synchronously. This function blocks until the bit is sent. Do not call from an interrupt handler! Returns true in case of success, false if an error occurs.

### beginWriteBit
`void beginWriteBit(bool value, bool repeat = false, void(*bitSent)(bool error) = 0)`

Sets a bit that will be sent next time the master asks for one. Optionnaly, the repeat parameter can be set to true to continue sending the same bit each time. In both cases, the send operation can be canceled by calling `stopWrite`.

### stopWrite
`void stopWrite()`

Cancels any pending write operation, started by writeBit or write. If this function is called before the master asked for a bit, then nothing is sent to the master.

### alarmed
`void alarmed(bool value)`

Sets the alarmed state, that is used when the master makes a conditional search of alarmed devices.

### setLogCallback(void(*callback)(const char* message))
`void setLogCallback(void(*callback)(const char* message))`

Sets (or replaces) a function to be called when the library has a message to log, if the functionality is enabled in OneWireSlave.cpp (uncomment line `#define ERROR_MESSAGES`). This is for debugging purposes.

### end
`void end()`

Stops all 1-wire activities, which frees hardware resources for other purposes.

## Notes about the interrupt-based implementation
Since the library is implemented using interrupts, none of its functions will block: you can continue execute your code immediately.

This also means callbacks are called from interrupt handlers, so you must make them very short to not block further communication.

You must also be careful when you explicitely block interrupts, as the 1-wire protocol has very tight timings, especially when writing bytes (which also happens when searching for device ROMs): a delay of 3 microseconds (yes, microseconds, not milliseconds) can be enough for some (quite intolerant) masters to miss a bit.

But if your code only blocks interrupts for reasonably short time, the probability to block exactly at the bad moment is low, so you can easily mitigate the issue by adding CRC checks in your high-level communication protocol, and retrying when an error is detected. This is an important thing to do anyway because 1-wire does not natively perform any error checking (excepted for ROM operations which already contain a CRC byte). Standard 1-wire devices also include CRC checks in their specific protocols.
