# OneWireArduinoSlave
An arduino library to communicate using the Dallas one-wire protocol, where the Arduino takes the role of a slave. Entirely implemented using interrupts, you can perform other tasks while communication is handled in background.

## 1-wire introduction
1-wire allows communication over long distances (100m and more, see Dallas documentation for details) with a single wire (plus a ground wire). You can put as much devices as you want on the same wire (they communicate one at a time). 1-wire also allows to send power over the data wire (parasitic power), but, though I haven't tried, I don't believe it would work with an Arduino. You'll need a separate 5V power source, which, if it comes next to your data wire, means you need 3 wires (5V, data, and ground). You'll also need a master controller, for example the USB adapter DS9490R, to connect to a computer, that will control communication with all 1-wire devices.

## How to use this library
Take a look at [the documentation of the library](./documentation.md)
