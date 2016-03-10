#ifndef _LowLevel_h
#define _LowLevel_h

#include <inttypes.h>

#if ARDUINO >= 100
#include "Arduino.h"       // for delayMicroseconds, digitalPinToBitMask, etc
#else
#include "WProgram.h"      // for delayMicroseconds
#include "pins_arduino.h"  // for digitalPinToBitMask, etc
#endif

#if defined(__AVR__)
#define PIN_TO_BASEREG(pin)             (portInputRegister(digitalPinToPort(pin)))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint8_t
#define IO_REG_ASM asm("r30")
#define DIRECT_READ(base, mask)         (((*(base)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*((base)+1)) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*((base)+1)) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*((base)+2)) &= ~(mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*((base)+2)) |= (mask))

#if defined (__AVR_ATtiny85__)
#define CLEARINTERRUPT GIFR |= (1 << INTF0)
#include "UserTimer.h" //ATtiny-support based on TinyCore1 Arduino-core for ATtiny at http://github.com/Coding-Badly/TinyCore1.git
__attribute__((always_inline)) static inline void UserTimer_Init( void )
{
	UserTimer_SetToPowerup();
	UserTimer_SetWaveformGenerationMode(UserTimer_(CTC_OCR));
}
__attribute__((always_inline)) static inline void UserTimer_Run(short skipTicks)
{
	UserTimer_SetCount(0);
	UserTimer_SetOutputCompareMatchAndClear(skipTicks);
	UserTimer_ClockSelect(UserTimer_(Prescale_Value_64));
}
#define UserTimer_Stop() UserTimer_ClockSelect(UserTimer_(Stopped))

#elif defined (__AVR_ATmega328P__)
#define CLEARINTERRUPT EIFR |= (1 << INTF0)
#define USERTIMER_COMPA_vect TIMER1_COMPA_vect

__attribute__((always_inline)) static inline void UserTimer_Init( void )
{
	TCCR1A = 0;
	TCCR1B = 0;
	// enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);
}
__attribute__((always_inline)) static inline void UserTimer_Run(short skipTicks)
{
	TCNT1 = 0;
	OCR1A = skipTicks;
	// turn on CTC mode with 64 prescaler
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
}
#define UserTimer_Stop() TCCR1B = 0
#endif

#elif defined(__MK20DX128__) || defined(__MK20DX256__)
#define PIN_TO_BASEREG(pin)             (portOutputRegister(pin))
#define PIN_TO_BITMASK(pin)             (1)
#define IO_REG_TYPE uint8_t
#define IO_REG_ASM
#define DIRECT_READ(base, mask)         (*((base)+512))
#define DIRECT_MODE_INPUT(base, mask)   (*((base)+640) = 0)
#define DIRECT_MODE_OUTPUT(base, mask)  (*((base)+640) = 1)
#define DIRECT_WRITE_LOW(base, mask)    (*((base)+256) = 1)
#define DIRECT_WRITE_HIGH(base, mask)   (*((base)+128) = 1)

#elif defined(__SAM3X8E__)
// Arduino 1.5.1 may have a bug in delayMicroseconds() on Arduino Due.
// http://arduino.cc/forum/index.php/topic,141030.msg1076268.html#msg1076268
// If you have trouble with OneWire on Arduino Due, please check the
// status of delayMicroseconds() before reporting a bug in OneWire!
#define PIN_TO_BASEREG(pin)             (&(digitalPinToPort(pin)->PIO_PER))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_ASM
#define DIRECT_READ(base, mask)         (((*((base)+15)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*((base)+5)) = (mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*((base)+4)) = (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*((base)+13)) = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*((base)+12)) = (mask))
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

#elif defined(__PIC32MX__)
#define PIN_TO_BASEREG(pin)             (portModeRegister(digitalPinToPort(pin)))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_ASM
#define DIRECT_READ(base, mask)         (((*(base+4)) & (mask)) ? 1 : 0)  //PORTX + 0x10
#define DIRECT_MODE_INPUT(base, mask)   ((*(base+2)) = (mask))            //TRISXSET + 0x08
#define DIRECT_MODE_OUTPUT(base, mask)  ((*(base+1)) = (mask))            //TRISXCLR + 0x04
#define DIRECT_WRITE_LOW(base, mask)    ((*(base+8+1)) = (mask))          //LATXCLR  + 0x24
#define DIRECT_WRITE_HIGH(base, mask)   ((*(base+8+2)) = (mask))          //LATXSET + 0x28

#else
#error "Please define I/O register types here"
#endif

class Pin
{
private:
	volatile IO_REG_TYPE *reg_;
	IO_REG_TYPE mask_;
	byte interruptNumber_;
	byte pinNumber_;

public:
	Pin()
		: mask_(0)
		, reg_(0)
		, interruptNumber_((byte)-1)
		, pinNumber_(255)
	{ }

	Pin(uint8_t pin)
	{
		pinNumber_ = pin;
		mask_ = PIN_TO_BITMASK(pin);
		reg_ = PIN_TO_BASEREG(pin);
		
		switch (pin)
		{
		case 2: interruptNumber_ = 0; break;
		case 3: interruptNumber_ = 1; break;
		default: interruptNumber_ = (byte)-1;
		}
	}

	inline byte getPinNumber() { return pinNumber_; }

	inline void inputMode() { DIRECT_MODE_INPUT(reg_, mask_); }
	inline void outputMode() { DIRECT_MODE_OUTPUT(reg_, mask_); }

	inline bool read() { return DIRECT_READ(reg_, mask_) == 1; }
	inline void writeLow() { DIRECT_WRITE_LOW(reg_, mask_); }
	inline void writeHigh() { DIRECT_WRITE_HIGH(reg_, mask_); }
	inline void write(bool value) { if (value) writeHigh(); else writeLow(); }

	inline void attachInterrupt(void (*handler)(), int mode)
	{
		CLEARINTERRUPT;  // clear any pending interrupt (we want to call the handler only for interrupts happending after it is attached)
		::attachInterrupt(interruptNumber_, handler, mode);
	}
	inline void detachInterrupt() { ::detachInterrupt(interruptNumber_); }
};

#endif
