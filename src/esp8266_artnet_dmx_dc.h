/*
 Original by
 https://github.com/robertoostenveld/arduino/tree/master/esp8266_artnet_dmx512
 Changes by
 https://github.com/Lutzion/
*/

#ifndef _ESP8266_ARTNET_DMX_DC_H_
#define _ESP8266_ARTNET_DMX_DC_H_

#include <ArtnetWifi.h> 

#ifdef USE_E131
//#include "_E131.h"
#include <ESPAsyncE131.h>
#endif

void LedBlack() ;
void LedRed() ;
void LedGreen() ;
void LedBlue() ;
void LedYellow() ;
void LedMagenta() ;
void LedCyan() ;

#ifdef POLL_REPLY
// https://art-net.org.uk/structure/discovery-packets/artpollreply/
typedef struct __attribute((__packed__)) _artnetPollReply
{
    unsigned char Artnet[8] ;
    unsigned char OpCodeLo ;
    unsigned char OpCodeHi ;
    unsigned char IpAdress[4] ;
    unsigned char PortNumberLo ;
    unsigned char PortNumberHi ;
    unsigned char VersInfoHi ;
    unsigned char VersInfoLo ;
    unsigned char NetSwitch ;
    unsigned char SubSwitch ;
    unsigned char OemHi ;
    unsigned char OemLo ;
    unsigned char UbeaVersion ;
    unsigned char Status1 ;
    unsigned char EstaManLo ;
    unsigned char EstaManHi ;
    unsigned char ShortName[18] ;
    unsigned char LongName[64] ;
    unsigned char NodeReport[64] ;
    unsigned char NumPortsHi ;
    unsigned char NumPortsLo ;
    unsigned char PortTypes[4] ;
    unsigned char GoodInput[4] ;
    unsigned char GoodOutput[4] ;
    unsigned char SwIn[4] ;
    unsigned char SwOut[4] ;
    unsigned char SwVideo ;
    unsigned char SwMacro ;
    unsigned char SwRemote ;
    unsigned char Spare[3] ;
    unsigned char Style ;
    unsigned char Mac[6] ;
    unsigned char BindIp[4] ;
    unsigned char BindIndex ;
    unsigned char Status2 ;
    unsigned char Filler[26] ;
} artnetPollReply ;


/*
=== ATTENTION: 
In 
/home/<user>/.platformio/lib/ArtnetWifi_ID6328/src/ArtnetWifi.h
do the following changes, to make Udp accessible.
You may have to repeat that after each ArtnetWifi updates

...
protected:
  WiFiUDP Udp;

private:
  uint16_t makePacket(void);

  //WiFiUDP Udp;
...
*/

class ArtnetWifiDC : public ArtnetWifi
{
    public:
      WiFiUDP inline UDP(void)
      {
        return Udp ;
      }
};
#endif // POLL_REPLY

#endif