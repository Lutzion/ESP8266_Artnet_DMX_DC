# ESP8266_Artnet_DMX_DC - Artnet to DMX with dimmer curves / mappings 
## artnet node that supports changing output values per channel and sequence check

This project is based on Robert Oostenvelds [esp8266_artnet_dmx512](https://github.com/robertoostenveld/arduino/tree/master/esp8266_artnet_dmx512).
It is not forked, but copied, as `esp8266_artnet_dmx512` is only part of the big repository [arduino](https://github.com/robertoostenveld/arduino)

ESP8266_Artnet_DMX_DC is the extended version of esp8266_artnet_dmx512 mainly to implement [dmxmapping](https://github.com/Lutzion/dmxmapping)

In addition i added checks for artnet sequence, which is important for all sorts of animations.

In UDP communication packets can change their order. When sequence is not checked, movements can make unwanted jumps. 