#include "Arduino.h"
#include "SerialChannel.h"

#define LEDPin    13
#define OWPin     2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

const int SkipSamples = 8; // how many samples we want to skip between two samples we keep (can be used to lower the sampling frequency)
const int BufferSize = 128;
byte buffer1[BufferSize];
byte buffer2[BufferSize];
byte* backBuffer = buffer1;
volatile byte backBufferPos = 0;
byte samplesSkipped = SkipSamples;
unsigned long backBufferStartTime = micros();

SerialChannel oscilloscope("oscilloscope");
SerialChannel debug("debug");

void setup()
{
    pinMode(LEDPin, OUTPUT); 
    pinMode(OWPin, INPUT);

    digitalWrite(LEDPin, LOW);

    attachInterrupt(InterruptNumber,onewireInterrupt,CHANGE);
    
    cli();//disable interrupts
  
    //set up continuous sampling of analog pin 0
    //clear ADCSRA and ADCSRB registers
    ADCSRA = 0;
    ADCSRB = 0;
    
    ADMUX |= (1 << REFS0); //set reference voltage
    ADMUX |= (1 << ADLAR); //left align the ADC value- so we can read highest 8 bits from ADCH register only
    
    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); //set ADC clock with 128 prescaler- 16mHz/128=125kHz ; 13 cycles for a conversion which means 9600 samples per second
    ADCSRA |= (1 << ADATE); //enabble auto trigger
    ADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
    ADCSRA |= (1 << ADEN); //enable ADC
    ADCSRA |= (1 << ADSC); //start ADC measurements
    
    sei();//enable interrupts
    
    Serial.begin(200000);
}

void loop()
{
    while(backBufferPos < BufferSize / 2) ;
    cli();//disable interrupts
    byte* currentBuffer = backBuffer;
    unsigned long currentBufferStartTime = backBufferStartTime;
    byte currentBufferSize = backBufferPos;
    backBuffer = (backBuffer == buffer1 ? buffer2 : buffer1);
    backBufferPos = 0;
    backBufferStartTime = micros();
    sei();//enable interrupts
    
    oscilloscope.write(currentBuffer, currentBufferSize, currentBufferStartTime);
}

ISR(ADC_vect) {//when new ADC value ready
    byte sample = ADCH; //store 8 bit value from analog pin 0
        
    if(samplesSkipped++ < SkipSamples)
        return;
    samplesSkipped = 0;

    backBuffer[backBufferPos++] = sample;
    if(backBufferPos >= BufferSize)
    {
        // overflow of back buffer, we loose the current sample
        backBufferPos = BufferSize - 1;
    }
}

void onewireInterrupt(void)
{
    //digitalWrite(LEDPin, digitalRead(OWPin));
}

