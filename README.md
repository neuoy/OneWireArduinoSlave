# OneWireArduinoSlave
An arduino library to communicate using the Dallas one-wire protocol, where the Arduino takes the role of a slave. Entirely implemented using interrupts, you can perform other tasks while communication is handled in background.

## 1-wire introduction
1-wire allows communication over long distances (100m and more, see Dallas documentation for details) with a single wire (plus a ground wire). You can put as much devices as you want on the same wire (they communicate one at a time). 1-wire also allows to send power over the data wire (parasitic power), but, though I haven't tried, I don't believe it would work with an Arduino. You'll need a separate 5V power source, which, if it comes next to your data wire, means you need 3 wires (5V, data, and ground). You'll also need a master controller, for example the USB adapter DS9490R, to connect to a computer, that will control communication with all 1-wire devices.

## How to use this library
This library allows you to emulate existing 1-wire devices with an Arduino, or to create your own protocol. All low-level details are handled by the library, such as reset detection, ROM matching, byte sending and receiving. Look at the demo sketch to see an example.

There are only 3 important functions:
- `void setReceiveCallback(void(*callback)(ReceiveEvent evt, byte data))` sets the function that will be called back when an event occurs, such as when a byte is received
- `void begin(byte* rom, byte pinNumber)` starts the library, which will respond to the provided ROM (must be unique on your network) on the specified Arduino pin. The ROM is cloned by the library, so you can discard your buffer immediately if you want.
- `void write(byte* bytes, short numBytes, void(*complete)(bool error))` starts writing one or more bytes, and will call the provided callback (optional) when it's done. The buffer you provide here must stay available until the end of the write operation, which happens in background. Do not use local variables.

## Notes about the interrupt-based implementation
Since the library is implemented using interrupts, none of its functions will block: you can continue execute your code immediately.

This also means callbacks are called from interrupt handlers, so you must make them very short to not block further communication.

You must also be careful when you explicitely block interrupts, as the 1-wire protocol has very tight timings, especially when writing bytes (which also happens when searching for device ROMs): a delay of 3 microseconds (yes, microseconds, not milliseconds) can be enough for some (quite intolerant) masters to miss a bit.

But if your code only blocks interrupts for reasonably short time, the probability to block exactly at the bad moment is low, so you can easily mitigate the issue by adding CRC checks in your high-level communication protocol, and retrying when an error is detected. This is an important thing to do anyway because 1-wire does not natively perform any error checking (excepted for ROM operations which already contain a CRC byte). Standard 1-wire devices also include CRC checks in their specific protocols.
