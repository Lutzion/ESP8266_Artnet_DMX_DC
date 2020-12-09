# CHANGELOG

18.11.2020
  - choose between WifiMangager and PersWifiManager (compiler switch PERS_WIFI) 

07.11.2020
  - sACN / E131 (ESPAsyncE131) as alternative receiver
  - statistics and webservice refresh for sACN
  - new parameters:
    - startDelay
    - e131Start
    - e131Unicast
    - psk

01.11.2020
  - new function __DATE__2Date()

18.08.2020
  - MultiResetDetector with validResetCallback function to show reset on LED

17.08.2020
  - show resetCounter in firmware version 

15.08.2020
  - call ArtnetPolled() after random time (max 1 seconds)
  - count resets with MultiResetDetector
  - control WifiManager with ResetButton
    - 2 times reset allows WifiManager
    - 5 times reset forces WifiManager

26.07.2020
  - gui with dark design and table in monitoring

25.07.2020
  - monitoring with webSockets

19.07.2020
  - waiting for connection to router depending on startDelay
  - SSID with ChipID (reverse as psk)

15.07.2020
  - finally LittleFS with  
     `board_build.filesystem = littlefs`  
    from version 2.6.0  
  - transferred ArtnetWifi in lib, to have  
     `protected: WiFiUDP Udp;`  
    in code  

12.06.2020
  - in ArtNetPolled() changes, for IPAdress.v4() type conversion

05.06.2020
  - prompt before deleting

04.06.2020
  - possibility to delete map-files
  - changing style sheet

29.05.2020
  - added ArtNetPolled()  
    using class `typedef struct __attribute((__packed__)) _artnetPollReply`  
  - ATTENTION:
    for easy reply possibility -> subclassed  
      class ArtnetWifiDC : public ArtnetWifi  
    that makes it neccessary to access Udp  
    so change  
     private: WiFiUDP Udp; 
    to  
     protected: WiFiUDP Udp;  
    in ArtnetWifi-library. You may have to repeat after each ArtnetWifi update  
  
    Artnet-Poll-Reply needed by programs like AuroraDMX  
     https://github.com/dfredell/AuroraDMX

26.05.2020
  - RGB LED  
    Colors:  
    - RED: no WLAN, no configuration, before reboot  
    - YELLOW: configuration read successful  
    - GREEN: normal mode  
    - BLUE: website called  
    - MAGENTA: no more Art-Net data for more than a second  
  - new file <esp8266_artnet_dmx_dc.h> for "export" of LED-functions  
  
15.05.2020
  - better feedback before / after upload, update, restart 

10.05.2020
  - new variables useMaps and mapChan for dmxmapping
  - output seqErr in %
  - dmxmapping implemented incl. upload of map-files

09.05.2020
  - a bit of gui - changing html pages
  - implement config.json in file system, to start with reasonable default values
  - in function onDmxPacket: checking universe and sequence before processing data

08.05.2020
  - new format for uptime
  