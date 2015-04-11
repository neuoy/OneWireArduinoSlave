#include "OWSlave.h"

#define LEDPin    13
#define OWPin     2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

unsigned char rom[8] = {0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00};

volatile long prevInt    = 0;      // Previous Interrupt micros
volatile boolean owReset = false;

OWSlave oneWire(OWPin); 

void setup()
{
    pinMode(LEDPin, OUTPUT); 
    pinMode(OWPin, INPUT);

    digitalWrite(LEDPin, LOW);

    attachInterrupt(InterruptNumber,onewireInterrupt,CHANGE);
    
    oneWire.setRom(rom);
}

void loop()
{
    if (owReset) owHandler();
}

void owHandler(void)
{
    detachInterrupt(InterruptNumber);
    owReset=false;
    
    if (oneWire.presence()) {
        if (oneWire.recvAndProcessCmd()) {
            uint8_t cmd = oneWire.recv();
            if (cmd == 0x44) {
                digitalWrite(LEDPin, HIGH);
            }
            if (cmd == 0xBE) {
                for( int i = 0; i < 9; i++) {
                    oneWire.send((byte)0);
                }
            }
        }
    }
    attachInterrupt(InterruptNumber,onewireInterrupt,CHANGE);
}

void onewireInterrupt(void)
{
    volatile long lastMicros = micros() - prevInt;
    prevInt = micros();
    if (lastMicros >= 410 && lastMicros <= 550)
    {
        //  OneWire Reset Detected
        owReset=true;
    }
}

