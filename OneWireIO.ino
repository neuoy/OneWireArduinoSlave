#include "Arduino.h"
#include "SerialChannel.h"

#define LEDPin    13
#define OWPin     2
#define InterruptNumber 0 // Must correspond to the OWPin to correctly detect state changes. On Arduino Uno, interrupt 0 is for digital pin 2

// how many samples we want to skip between two samples we keep (can be used to lower the sampling frequency)
#define SkipSamples 0
byte regularEncodedFrequency;
byte burstEncodedFrequency;

int regularADCSRA;
int burstADCSRA;

const int BufferSize = 128;
const int BurstBufferSize = 1024;
byte buffer1[BufferSize];
byte buffer2[BufferSize];
byte burstBuffer[BurstBufferSize];
volatile byte* backBuffer = buffer1;
volatile short backBufferPos = 0;
byte samplesSkipped = SkipSamples;
volatile unsigned long backBufferStartTime = micros();

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
    
    byte skipSamples = 0;
    #if SkipSamples > 0
    skipSamples = SkipSamples;
    #endif
    
    int ADPS = (1 << ADPS2) | (0 << ADPS1) | (1 << ADPS0);
    regularADCSRA = 0;
    regularADCSRA |= ADPS; //set ADC clock with 32 prescaler- 16mHz/32=500KHz ; 13 cycles for a conversion which means 38000 samples per second
    regularADCSRA |= (1 << ADATE); //enabble auto trigger
    regularADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
    regularADCSRA |= (1 << ADEN); //enable ADC
    regularADCSRA |= (1 << ADSC); //start ADC measurements

    regularEncodedFrequency = (byte)ADPS;
    regularEncodedFrequency |= skipSamples << 3;
    
    ADCSRA = regularADCSRA;
    
    ADPS = (0 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    burstADCSRA = 0;
    burstADCSRA |= ADPS; //set ADC clock with 32 prescaler- 16mHz/32=500KHz ; 13 cycles for a conversion which means 38000 samples per second
    burstADCSRA |= (1 << ADATE); //enabble auto trigger
    burstADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
    burstADCSRA |= (1 << ADEN); //enable ADC
    burstADCSRA |= (1 << ADSC); //start ADC measurements

    burstEncodedFrequency = (byte)ADPS;
    burstEncodedFrequency |= skipSamples << 3;
    
    sei();//enable interrupts
    
    Serial.begin(400000);
}

void loop()
{
    while(backBufferPos < BufferSize / 2 || (backBuffer == burstBuffer && backBufferPos < BurstBufferSize - 1)) ;
    cli();//disable interrupts
    byte* currentBuffer = (byte*)backBuffer;
    short currentBufferSize = backBufferPos;
    backBuffer = (backBuffer == buffer1 ? buffer2 : buffer1);
    backBufferPos = 0;
    if(currentBuffer == burstBuffer)
    {
        ADCSRA = regularADCSRA;
    }
    sei();//enable interrupts
    unsigned long currentBufferStartTime = backBufferStartTime;
    backBufferStartTime = micros();
    digitalWrite(LEDPin, LOW);
    
    byte encodedFrequency = currentBuffer == burstBuffer ? burstEncodedFrequency : regularEncodedFrequency;
    
    //Serial.write(currentBuffer, currentBufferSize);
    oscilloscope.beginWrite(currentBufferSize + 1, currentBufferStartTime);
    oscilloscope.continueWrite(&encodedFrequency, 1);
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
    if(backBuffer == burstBuffer)
    {
        if(backBufferPos >= BurstBufferSize)
        {
            backBufferPos = BurstBufferSize - 1;
        }
    }
    else
    {
        if(backBufferPos >= BufferSize)
        {
            // overflow of back buffer, we loose the current sample
            digitalWrite(LEDPin, HIGH);
            backBufferPos = BufferSize - 1;
        }
    }
    
    // switch to burst mode if the trigger condition is met
    /*if(backBuffer != burstBuffer && sample < 127)
    {
        backBuffer = burstBuffer;
        ADCSRA = burstADCSRA;
        backBufferPos = 0;
        backBufferStartTime = micros();
    }*/
}

void onewireInterrupt(void)
{
    //digitalWrite(LEDPin, digitalRead(OWPin));
}

