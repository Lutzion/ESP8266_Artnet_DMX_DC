# COMPILING

This project is created with platformio.  
Before compiling, you have to download the required libraries:
- ArduinoJson
- Time
- Uptime Library
- WifiManager
- ESPAsyncE131
- DNSServer
- dmxmapping  
  
- slightly modified versions of ArtnetWifi and PersWifiManager are included
  

`platformio.ini` is configured for a `Wemos d1_mini` board.  
If you use another one, change settings in `platformio.ini`. 

After compiling upload the `firmware.bin`.
Then you have to upload the file system image. 

Uploading the file image via menu does not work in some versions of platformio. If so, use command line:  
`~/.platformio/penv/bin/pio run -t uploadfs`

This uploads the files of the `data` directory. These files are important for the web server and contain some examples for dmxmapping.  
If you want to use the funktions of [dmxmapping](https://github.com/Lutzion/dmxmapping), change the `*.map` files according to your needs.  
  
## Compiler switches
These are the `build_flags` available in this project: 

```
build_flags = -ggdb3  
              -D LITTLEFS         ; new file system  
              -D VERBOSE          ; some debug info  
              -D MAPS             ; use dmxmapping / dimmer curves  
              ;-D VERBOSE_MAP     ; activate debug for dmxmapping  
              ;-D DMX_DEBUG       ; activate debug for DMX  
              -D POLL_REPLY       ; activate ARTNET_POLL_REPLY  
              -D WEBSOCKET        ; activate WEBSOCKET for statistics  
              -D MRD_ESP8266_RTC  ; use multiresetdetector for counting resets  
              -D USE_E131         ; also use E131 protocol  
              ;-D E131_VERBOSE    ; activate E131 debug  
              -D PERS_WIFI        ; PersWifiManager instead of WifiManager
```

### LITTLEFS
platformio recommends to use LittleFS instead of SPIFFS. If you uncomment `-D LITTLEFS` LittleFS is used.  
  
### VERBOSE
uncomment `-D VERBOSE` to watch some informations with serial console when developing and debugging the firmware.  
  
### MAPS 
uncomment `-D MAPS` to include [dmxmapping](https://github.com/Lutzion/dmxmapping) in this project.  
  
### VERBOSE_MAP
to activate serial debugging for dmxmapping uncomment `-D VERBOSE_MAP`.  
  
### DMX_DEBUG
to activate serial debugging for dmx values uncomment `-D DMX_DEBUG`.  
  
### POLL_REPLY
uncomment `-D POLL_REPLY` to make the firmware answer to ArtPoll packets. For example [AuroraDMX](https://github.com/dfredell/AuroraDMX) uses ArtPoll packets to find interfaces to send ArtNet to.  
  
### WEBSOCKET
To refresh monitor page in "real time" without loading the complete page uncomment `-D WEBSOCKET`.  
  
### MRD_ESP8266_RTC
MultiResetDetector counts how often the reset button is pressed while (re)starting.  
If `-D MRD_ESP8266_RTC` is uncommented, the reset button has to be pressed at least 2 times to make it possible to change wifi settings.  
  
### USE_E131
An alternative to ArtNet is E1.31 / sACN. To enable the firmware to receive E1.31 / sACN as well uncomment `-D USE_E131`.  
  
### E131_VERBOSE
like in previous switches uncomment `-D E131_VERBOSE` to enable serial debugging for E131.  
  
### PERS_WIFI
choose between WifiManager or PersWifiManager: uncomment `-D PERS_WIFI` to use PersWifiManager.  With PersWifiManager you are able to use to complete functionality while the device is in AP-mode (no connection to router available, only connection between artnet-software and the device.)
  
  
[Back to main page](README.md)