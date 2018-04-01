This is a **DS18B20** emulator, i.e. it turns an Arduino Uno into a device roughly equivalent to an actual DS18B20 temperature sensor. Excepted in this sample it doesn't actually sense temperature (because the Arduino lacks the hardware for this), it just returns 42 as if that was the expected answer.

To make it work, you will need to connect your Arduino Uno to a 1-wire master (for example an USB adapter on a computer).

You can then connect:

- a GND pin of your Uno to your 1-wire network ground.
- the pin "2" of your Uno to the data line of your 1-wire network. You may use "3" as well but need to change the code.

You will see 28.000000000002 probe that always return 42 as temperature :

    $ cat 28.000000000002/temperature
    42
Conversion timing is emulated as well.

If you don't use an Arduino Uno, you may need to adjust which pin you connect as the data line (it needs to have hardware interrupts), and make the corresponding change in the example source code. A few microcontrollers are compatible with the library, check the full list in [the documentation](../../extras/documentation.md).