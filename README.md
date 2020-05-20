# ESP8266_Artnet_DMX_DC - Artnet to DMX with dimmer curves / mappings 
## artnet node that supports changing output values per channel and sequence check

This project is based on Robert Oostenvelds [esp8266_artnet_dmx512](https://github.com/robertoostenveld/arduino/tree/master/esp8266_artnet_dmx512).
It is not forked, but copied, as `esp8266_artnet_dmx512` is part of the big repository [arduino](https://github.com/robertoostenveld/arduino).

ESP8266_Artnet_DMX_DC is the extended version of esp8266_artnet_dmx512 mainly to implement [dmxmapping](https://github.com/Lutzion/dmxmapping).


## Sequence check
In addition i added checks for artnet sequence, which is important for all sorts of animations.

In UDP communication packets can change their order. When sequence is not checked, movements can make unwanted jumps.

![Monitor!](/media/Monitor.png "Monitor page")

If monitor shows too many sequence errors, maybe check your network configurations and dmx software settings.

Calling the web interface might create sequence errors, as the other functions stop for a short time! 

## Dimmer curves / dmx mapping
To implement mappings there are 2 more settings in the devices settings page.

![Settings!](/media/Settings.png "Settings page")

- useMaps: global setting for activating mappings. 0: deactivated, 1 activated
- mapChan: if useMaps is 0 and mapChan is not 0 then the mapChan value is the dmx channel, that is watched to switch mapping on and off according to the value of the dmx channel. If the value is smaller 127, mapping is off, otherwise it is on.

## The service menu
### Show map files
With the menu `Show map files` in the service menu you can have a look at the existing map files.
![DirMap!](/media/DirMapFiles.png "Directory of map files")

### Upload map files
The menu `Upload map files` allows it to select a map file and upload it.




# ATTENTION
## When calling the web interface, dmx output might stop for some seconds.
## Thus do not call the web interface while using the interface for a live show! 

