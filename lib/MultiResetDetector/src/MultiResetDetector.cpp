#include "MultiResetDetector.h"

// https://github.com/renearts/ESP8266-P1-transmitter/blob/master/CRC16.h
uint16_t CRC16(uint16_t crc, uint8_t *buf, int len)
{
  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (unsigned int)buf[pos];    // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }

  return crc;
}

/*
MultiResetDetector::MultiResetDetector(uint32_t timeoutMs, uint32_t eepromSector) :
  timeoutMs(timeoutMs),
  waitingForMultiReset(true)
{
    setValuableResetReasons({REASON_DEFAULT_RST, REASON_EXT_SYS_RST});
#ifdef MRD_ESP8266_RTC
    rtcAddress = eepromSector ;
#else
    eeprom.begin((eepromSector + 1) * SPI_FLASH_SEC_SIZE);
#endif
}
*/

MultiResetDetector::MultiResetDetector(uint32_t timeoutMs, uint32_t eepromSector, void (*fptr)()) :
  timeoutMs(timeoutMs),
  waitingForMultiReset(true),
  validResetCallback(fptr)
{
    setValuableResetReasons({/*REASON_DEFAULT_RST,*/ REASON_EXT_SYS_RST});
#ifdef MRD_ESP8266_RTC
    rtcAddress = eepromSector ;
#else
    eeprom.begin((eepromSector + 1) * SPI_FLASH_SEC_SIZE);
#endif
}

MultiResetDetector::~MultiResetDetector()
{
#ifdef MRD_ESP8266_RTC

#else
  eeprom.end();
#endif
}

MultiResetDetector& MultiResetDetector::setValuableResetReasons(std::initializer_list<rst_reason> reasons) {
  isLastResetReasonValuable = std::end(reasons) != std::find(
    std::begin(reasons), std::end(reasons), system_get_rst_info()->reason);
  return *this;
}

uint8_t MultiResetDetector::readResetCount()
{
#ifdef MRD_ESP8266_RTC
    ESP.rtcUserMemoryRead(rtcAddress, (uint32_t *)&rtcData, sizeof(rtcData)); 
    uint16_t crc = CRC16(0, (uint8_t *)&rtcData.data, sizeof(rtcData.data));

#if MRD_DEBUG
    char cDebug[64] ;
    sprintf(cDebug, "MultiResetDetector::readResetCount %d %04X - %04X", 
                    rtcData.data, rtcData.crc, crc);
    Serial.println(cDebug) ;                
#endif

    if (crc != rtcData.crc)
    {
#if MRD_DEBUG
      Serial.println("MultiResetDetector - CRC Fehler") ;
#endif
      return 0 ;
    }

    return  rtcData.data ;
#else
  uint8_t const *eepromBegin = eeprom.getConstDataPtr();
  uint8_t const *eepromEnd = eepromBegin + SPI_FLASH_SEC_SIZE;
  uint8_t const *resetCount = nullptr;
  for (uint8_t const *cell = eepromBegin; cell < eepromEnd; ++cell) {
    if (*cell == 0xFF) {
      continue;
    }
    if (resetCount != nullptr) {
      return 0;
    }
    resetCount = cell;
  }
  return resetCount == nullptr ? 0 : *resetCount;
#endif
}

void MultiResetDetector::writeResetCount(uint8_t resetCount)
{
#ifdef MRD_ESP8266_RTC
  //MULTIRESET_COUNTER = resetCount ;
  rtcData.data = resetCount ;
  rtcData.crc = CRC16(0, (uint8_t *)&rtcData.data, sizeof(rtcData.data));

  ESP.rtcUserMemoryWrite(rtcAddress, (uint32_t *)&rtcData, sizeof(rtcData));
#else
  uint8_t *eepromBegin = eeprom.getDataPtr();
  uint8_t *eepromEnd = eepromBegin + SPI_FLASH_SEC_SIZE;
  uint8_t *resetCountCellPtr = nullptr;
  for (uint8_t *cell = eepromBegin; cell < eepromEnd; ++cell) {
    if (*cell == 0xFF) {
      continue;
    }
    if (resetCountCellPtr == nullptr) {
      resetCountCellPtr = cell;
    }
    *cell = 0xFF;
  }
  if (resetCountCellPtr == nullptr || ++resetCountCellPtr >= eepromEnd) {
    resetCountCellPtr = eepromBegin;
  }
  *resetCountCellPtr = resetCount;
  eeprom.commit();
#endif
}

uint8_t MultiResetDetector::detectResetCount()
{
  uint8_t resetCount = readResetCount();
  if (isLastResetReasonValuable)
  {
    writeResetCount(++resetCount);
    // show valid reset by calling the function
    if (validResetCallback != NULL)
    {
      validResetCallback() ;
    }
  }
  return resetCount;
}

bool MultiResetDetector::handle()
{
  if (!waitingForMultiReset || !isLastResetReasonValuable)
  {
    return false;
  }
  if (millis() > timeoutMs)
  {
    waitingForMultiReset = false;
    writeResetCount(0);
  }
  return true;
}

void MultiResetDetector::finishMonitoring()
{
  while (handle()) 
  {
    yield();
  }
}

uint8_t MultiResetDetector::execute()
{
  uint8_t resetCount = detectResetCount();
  finishMonitoring();
  return resetCount;
}
/*
uint8_t MultiResetDetector::execute(uint32_t timeoutMs, uint32_t eepromSector)
{
  return MultiResetDetector(timeoutMs, eepromSector).execute();
}
*/

uint8_t MultiResetDetector::execute(uint32_t timeoutMs, uint32_t eepromSector, void (*fptr)())
{
  return MultiResetDetector(timeoutMs, eepromSector, fptr).execute();
}
