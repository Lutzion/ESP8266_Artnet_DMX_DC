#ifndef MultiResetDetector_H__
#define MultiResetDetector_H__

#include <Arduino.h>
#include <user_interface.h>
#include <EEPROM.h>

struct _rtcData
{
  uint16_t  data ;
  uint16_t  crc ;
} ;

class MultiResetDetector {
  uint32_t timeoutMs;
  bool waitingForMultiReset;
  bool isLastResetReasonValuable;
  EEPROMClass eeprom;
public:
  //MultiResetDetector(uint32_t timeoutMs, uint32_t eepromSector = 0);
  MultiResetDetector(uint32_t timeoutMs, uint32_t eepromSector = 0, void (*fptr)() = NULL);
  ~MultiResetDetector();

  /**
   * Resets with the reason different to provide are ignored.
   */
  MultiResetDetector& setValuableResetReasons(std::initializer_list<rst_reason> reasons);

  /**
   * Starts blocking detection. Returns how many times microcontroller was reseted.
   */
  uint8_t execute();

  /**
   * Detscts the cause of device reset and increase the counter. Should be 
   * called on the first line of setup method and only once.
   */
  uint8_t detectResetCount();

  /**
   * Blocks until monitoring time interval is up. 
   */ 
  void finishMonitoring();

  /**
   * Ensures that reset counter set to zero. Returns whether monitoring time
   * interval is up. Could be used inside a loop instead of blocking 
   * finishMonitoring if timeoutMs is too big.
   */
  bool handle();

  /**
   * Starts detection and waits timeoutMs. Returns how many times reset button
   * was clicked.
   */
  //static uint8_t execute(uint32_t timeoutMs, uint32_t eepromSector = 0);
  static uint8_t execute(uint32_t timeoutMs, uint32_t eepromSector = 0, void (*fptr)() = NULL);

  inline void setValidResetCallback(void (*fptr)())
  {
    validResetCallback = fptr;
  }

private:
#ifdef MRD_ESP8266_RTC
  //uint32_t MULTIRESET_COUNTER;
  _rtcData rtcData ;
  uint32_t rtcAddress;
#endif

  uint8_t readResetCount();
  void writeResetCount(uint8_t resetCount);

  void (*validResetCallback)();
};
#endif // MultiResetDetector_H__