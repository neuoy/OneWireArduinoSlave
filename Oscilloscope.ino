#include "Arduino.h"
#include "SerialChannel.h"

#define LEDPin    13
#define OWPin     2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

// how many samples we want to skip between two samples we keep (can be used to lower the sampling frequency)
#define SkipSamples 0
byte regularEncodedFrequency;

const int BufferSize = 512;
byte buffer1[BufferSize];
byte buffer2[BufferSize];
byte* backBuffer = buffer1;
volatile short backBufferPos = 0;
byte samplesSkipped = SkipSamples;
unsigned long backBufferStartTime = micros();

SerialChannel oscilloscope("oscilloscope");
SerialChannel debug("debug");

void setup()
{
    pinMode(LEDPin, OUTPUT); 
    pinMode(OWPin, INPUT);

    digitalWrite(LEDPin, LOW);

    //attachInterrupt(InterruptNumber,onewireInterrupt,CHANGE);
    
    cli();//disable interrupts
  
    //set up continuous sampling of analog pin 0
    //clear ADCSRA and ADCSRB registers
    ADCSRA = 0;
    ADCSRB = 0;
    
    ADMUX |= (1 << REFS0); //set reference voltage
    ADMUX |= (1 << ADLAR); //left align the ADC value- so we can read highest 8 bits from ADCH register only
    
	int ADPS = (1 << ADPS2) | (0 << ADPS1) | (1 << ADPS0);
    ADCSRA |= ADPS; //set ADC clock with 32 prescaler- 16mHz/32=500KHz ; 13 cycles for a conversion which means 38000 samples per second
    ADCSRA |= (1 << ADATE); //enabble auto trigger
    ADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
    ADCSRA |= (1 << ADEN); //enable ADC
    ADCSRA |= (1 << ADSC); //start ADC measurements

	regularEncodedFrequency = (byte)ADPS;
	byte skipSamples = 0;
	#if SkipSamples > 0
	skipSamples = SkipSamples;
	#endif
	regularEncodedFrequency |= skipSamples << 3;
    
    sei();//enable interrupts
    
    Serial.begin(400000);
}

void loop()
{
    while(backBufferPos < BufferSize / 2) ;
    cli();//disable interrupts
    byte* currentBuffer = backBuffer;
    short currentBufferSize = backBufferPos;
    backBuffer = (backBuffer == buffer1 ? buffer2 : buffer1);
    backBufferPos = 0;
    sei();//enable interrupts
    unsigned long currentBufferStartTime = backBufferStartTime;
    backBufferStartTime = micros();
    digitalWrite(LEDPin, LOW);
    
    //Serial.write(currentBuffer, currentBufferSize);
    oscilloscope.beginWrite(currentBufferSize + 1, currentBufferStartTime);
    oscilloscope.continueWrite(&regularEncodedFrequency, 1);
    oscilloscope.continueWrite(currentBuffer, currentBufferSize);
}

ISR(ADC_vect) {//when new ADC value ready
    byte sample = ADCH; //store 8 bit value from analog pin 0
    
    #if SkipSamples > 0
    if(samplesSkipped++ < SkipSamples)
        return;
    samplesSkipped = 0;
    #endif

    backBuffer[backBufferPos++] = sample;
    if(backBufferPos >= BufferSize)
    {
        // overflow of back buffer, we loose the current sample
        digitalWrite(LEDPin, HIGH);
        backBufferPos = BufferSize - 1;
    }
}

void onewireInterrupt(void)
{
    //digitalWrite(LEDPin, digitalRead(OWPin));
}

