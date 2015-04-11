/*
OWSlave v1.0 by Alexander Gordeyev

It is based on Jim's Studt OneWire library v2.0

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Much of the code was inspired by Derek Yerger's code, though I don't
think much of that remains.  In any event that was..
    (copyleft) 2006 by Derek Yerger - Free to distribute freely.

The CRC code was excerpted and inspired by the Dallas Semiconductor
sample code bearing this copyright.
//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//--------------------------------------------------------------------------
*/

#include "OWSlave.h"
#include "pins_arduino.h"

extern "C" {
// #include "WConstants.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
}

#define DIRECT_READ(base, mask)        (((*(base)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)  ((*(base+1)) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask) ((*(base+1)) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)   ((*(base+2)) &= ~(mask))
#define DIRECT_WRITE_HIGH(base, mask)  ((*(base+2)) |= (mask))

#define TIMESLOT_WAIT_RETRY_COUNT microsecondsToClockCycles(120) / 10L

OWSlave::OWSlave(uint8_t pin) {
    pin_bitmask = digitalPinToBitMask(pin);
    baseReg = portInputRegister(digitalPinToPort(pin));
}

void OWSlave::setRom(unsigned char rom[8]) {
    for (int i=0; i<7; i++)
        this->rom[i] = rom[i];
    this->rom[7] = crc8(this->rom, 7);
}

void OWSlave::setRomnc(unsigned char rom[8]) {
   for (int i=0; i<8; i++)
        this->rom[i] = rom[i];
}

bool OWSlave::waitForRequest(bool ignore_errors) {
    errno = ONEWIRE_NO_ERROR;
    for (;;) {
        if (!waitReset(0) )
            continue;
        if (!presence() )
            continue;
        if (recvAndProcessCmd() )
            return TRUE;
        else if ((errno == ONEWIRE_NO_ERROR) || ignore_errors)
            continue;
        else
            return FALSE;
    }
}

bool OWSlave::recvAndProcessCmd() {
    char addr[8];

    for (;;) {
      switch (recv() ) {
        case 0xF0: // SEARCH ROM
            search();
            return FALSE;
        case 0x33: // READ ROM
            sendData(rom, 8);
            if (errno != ONEWIRE_NO_ERROR)
                return FALSE;
            break;
        case 0x55: // MATCH ROM
            recvData(addr, 8);
            if (errno != ONEWIRE_NO_ERROR)
                return FALSE;
            for (int i=0; i<8; i++)
                if (rom[i] != addr[i])
                    return FALSE;
            return TRUE;
        case 0xCC: // SKIP ROM
            return TRUE;
        default: // Unknown command
            if (errno == ONEWIRE_NO_ERROR)
                return FALSE;
            else
                return FALSE;
      }
    }
}

bool OWSlave::search() {
    uint8_t bitmask;
    uint8_t bit_send, bit_recv;

    for (int i=0; i<8; i++) {
        for (bitmask = 0x01; bitmask; bitmask <<= 1) {
            bit_send = (bitmask & rom[i])?1:0;
            sendBit(bit_send);
            sendBit(!bit_send);
            bit_recv = recvBit();
            if (errno != ONEWIRE_NO_ERROR)
                return FALSE;
            if (bit_recv != bit_send)
                return FALSE;
        }
    }
    return TRUE;
}

bool OWSlave::waitReset(uint16_t timeout_ms) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;
    unsigned long time_stamp;

    errno = ONEWIRE_NO_ERROR;
    cli();
    DIRECT_MODE_INPUT(reg, mask);
    sei();
    if (timeout_ms != 0) {
        time_stamp = micros() + timeout_ms*1000;
        while (DIRECT_READ(reg, mask)) {
            if (micros() > time_stamp) {
                errno = ONEWIRE_WAIT_RESET_TIMEOUT;
                return FALSE;
            }
        }
    } else
        while (DIRECT_READ(reg, mask)) {};
    time_stamp = micros() + 540;
    while (DIRECT_READ(reg, mask) == 0) {
        if (micros() > time_stamp) {
            errno = ONEWIRE_VERY_LONG_RESET;
            return FALSE;
        }
    }
    if ((time_stamp - micros()) > 70) {
        errno = ONEWIRE_VERY_SHORT_RESET;
        return FALSE;
    }
    delayMicroseconds(30);
    return TRUE;
}
bool OWSlave::waitReset() {
    return waitReset(1000);
}

bool OWSlave::presence(uint8_t delta) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;

    errno = ONEWIRE_NO_ERROR;
    cli();
    DIRECT_WRITE_LOW(reg, mask);
    DIRECT_MODE_OUTPUT(reg, mask);    // drive output low
    sei();
    delayMicroseconds(120);
    cli();
    DIRECT_MODE_INPUT(reg, mask);     // allow it to float
    sei();
    delayMicroseconds(300 - delta);
    if ( !DIRECT_READ(reg, mask)) {
        errno = ONEWIRE_PRESENCE_LOW_ON_LINE;
        return FALSE;
    } else
        return TRUE;
}
bool OWSlave::presence() {
    return presence(25);
}

uint8_t OWSlave::sendData(char buf[], uint8_t len) {
    uint8_t bytes_sended = 0;

    for (int i=0; i<len; i++) {
        send(buf[i]);
        if (errno != ONEWIRE_NO_ERROR)
            break;
        bytes_sended++;
    }
    return bytes_sended;
}

uint8_t OWSlave::recvData(char buf[], uint8_t len) {
    uint8_t bytes_received = 0;
    
    for (int i=0; i<len; i++) {
        buf[i] = recv();
        if (errno != ONEWIRE_NO_ERROR)
            break;
        bytes_received++;
    }
    return bytes_received;
}

void OWSlave::send(uint8_t v) {
    errno = ONEWIRE_NO_ERROR;
    for (uint8_t bitmask = 0x01; bitmask && (errno == ONEWIRE_NO_ERROR); bitmask <<= 1)
        sendBit((bitmask & v)?1:0);
}

uint8_t OWSlave::recv() {
    uint8_t r = 0;

    errno = ONEWIRE_NO_ERROR;
    for (uint8_t bitmask = 0x01; bitmask && (errno == ONEWIRE_NO_ERROR); bitmask <<= 1)
        if (recvBit())
            r |= bitmask;
    return r;
}

void OWSlave::sendBit(uint8_t v) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;

    cli();
    DIRECT_MODE_INPUT(reg, mask);
    if (!waitTimeSlot() ) {
        errno = ONEWIRE_WRITE_TIMESLOT_TIMEOUT;
        sei();
        return;
    }
    if (v & 1)
        delayMicroseconds(50);
    else {
        cli();
        DIRECT_WRITE_LOW(reg, mask);
        DIRECT_MODE_OUTPUT(reg, mask);
        delayMicroseconds(50);
        DIRECT_WRITE_HIGH(reg, mask);
        sei();
    }
    sei();
    return;
}

uint8_t OWSlave::recvBit(void) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;
    uint8_t r;

    cli();
    DIRECT_MODE_INPUT(reg, mask);
    if (!waitTimeSlot() ) {
        errno = ONEWIRE_READ_TIMESLOT_TIMEOUT;
        sei();
        return 0;
    }
    delayMicroseconds(30);
    r = DIRECT_READ(reg, mask);
    sei();
    return r;
}

bool OWSlave::waitTimeSlot() {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;
    uint16_t retries;

    retries = TIMESLOT_WAIT_RETRY_COUNT;
    while ( !DIRECT_READ(reg, mask))
        if (--retries == 0)
            return FALSE;
    retries = TIMESLOT_WAIT_RETRY_COUNT;
    while ( DIRECT_READ(reg, mask))
        if (--retries == 0)
            return FALSE;
    return TRUE;
}

#if OWSlave_CRC
// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
//

#if OWSlave_CRC8_TABLE
// This table comes from Dallas sample code where it is freely reusable,
// though Copyright (C) 2000 Dallas Semiconductor Corporation
static const uint8_t PROGMEM dscrc_table[] = {
      0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
    157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
     35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
    190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
     70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
    219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
    101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
    248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
    140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
     17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
    175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
     50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
    202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
     87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
    233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
    116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//
// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (note: this might better be done without to
// table, it would probably be smaller and certainly fast enough
// compared to all those delayMicrosecond() calls.  But I got
// confused, so I use this table from the examples.)
//
uint8_t OWSlave::crc8(char addr[], uint8_t len)
{
    uint8_t crc = 0;

    while (len--) {
        crc = pgm_read_byte(dscrc_table + (crc ^ *addr++));
    }
    return crc;
}
#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
//
uint8_t OWSlave::crc8(char addr[], uint8_t len)
{
    uint8_t crc = 0;
    
    while (len--) {
        uint8_t inbyte = *addr++;
        for (uint8_t i = 8; i; i--) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}
#endif

#endif
