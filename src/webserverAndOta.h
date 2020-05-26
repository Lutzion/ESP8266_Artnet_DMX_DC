/*
 Original by
 https://github.com/robertoostenveld/arduino/tree/master/esp8266_artnet_dmx512
 Changes by
 https://github.com/Lutzion/
*/

#ifndef _WEBSERVER_AND_OTA_H_
#define _WEBSERVER_AND_OTA_H_

#ifdef LITTLEFS
#include <LittleFS.h>
#else
#include <FS.h>
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>


#define JSON_TO_CONFIG(x, y)   { if (root.containsKey(y)) { config.x = root[y]; } }
#define CONFIG_TO_JSON(x, y)   { root[y] = config.x; }
#define KEYVAL_TO_CONFIG(x, y) { if (server.hasArg(y))    { String str = server.arg(y); config.x = str.toInt(); } }

#define S_JSON_TO_CONFIG(x, y)   { if (root.containsKey(y)) { strcpy(config.x, root[y]); } }
#define S_CONFIG_TO_JSON(x, y)   { root.set(y, config.x); }
#define S_KEYVAL_TO_CONFIG(x, y) { if (server.hasArg(y))    { String str = server.arg(y); strcpy(config.x, str.c_str()); } }

bool initialConfig(void);
bool loadConfig(void);
bool saveConfig(void);

void finalizeWebServer(uint16_t);

void handleMapUploadFn(void);
void handleMapUploadUpload(void);
void handleFwUpdateFn(void);
void handleFwUpdateUpload(void);
void handleDirList(bool);
void handleNotFound(void);
void handleRedirect(String);
void handleRedirect(const char *);
bool handleStaticFile(String);
bool handleStaticFile(const char *);
void handleJSON();

// configuration with default values
struct Config
{
  uint8_t universe = 0;
  uint16_t channels = 512;
  uint16_t delay = 25 ; 
  uint8_t useMaps = 0;
  uint16_t mapChan = 0;
};

class file
{
  public :
    String name ;
    size_t size;
    time_t time;
    time_t creationTime;
};

#endif // _WEBSERVER_AND_OTA_H_
