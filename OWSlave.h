#ifndef iButton_h
#define iButton_h

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif 

#include <inttypes.h>

// You can exclude CRC checks altogether by defining this to 0
#ifndef OWSlave_CRC
#define OWSlave_CRC 1
#endif

// Select the table-lookup method of computing the 8-bit CRC
// by setting this to 1.  The lookup table no longer consumes
// limited RAM, but enlarges total code size by about 250 bytes
#ifndef OWSlave_CRC8_TABLE
#define OWSlave_CRC8_TABLE 0
#endif

#define FALSE 0
#define TRUE  1

#define ONEWIRE_NO_ERROR 0
#define ONEWIRE_READ_TIMESLOT_TIMEOUT 1
#define ONEWIRE_WRITE_TIMESLOT_TIMEOUT 2
#define ONEWIRE_WAIT_RESET_TIMEOUT 3
#define ONEWIRE_VERY_LONG_RESET 4
#define ONEWIRE_VERY_SHORT_RESET 5
#define ONEWIRE_PRESENCE_LOW_ON_LINE 6

class OWSlave {
  private:
    bool waitTimeSlot();
    uint8_t pin_bitmask;
    volatile uint8_t *baseReg;
    char rom[8];
  public:
    bool recvAndProcessCmd();
    OWSlave(uint8_t pin);
    void setRom(unsigned char rom[8]);
    void setRomnc(unsigned char rom[8]);
    bool waitForRequest(bool ignore_errors);
    bool waitReset(uint16_t timeout_ms);
    bool waitReset();
    bool presence(uint8_t delta);
    bool presence();
    bool search();
    uint8_t sendData(char buf[], uint8_t data_len);
    uint8_t recvData(char buf[], uint8_t data_len);
    void send(uint8_t v);
    uint8_t recv(void);
    void sendBit(uint8_t v);
    uint8_t recvBit(void);
#if OWSlave_CRC
    static uint8_t crc8(char addr[], uint8_t len);
#endif
    uint8_t errno;
};

#endif
