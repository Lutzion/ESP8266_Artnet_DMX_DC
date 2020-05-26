/*
  Original by
  https://github.com/robertoostenveld/arduino/tree/master/esp8266_artnet_dmx512
  Changes by
  https://github.com/Lutzion/


  This sketch receives Art-Net data of one DMX universes over WiFi
  and sends it over the serial interface to a MAX485 module.

  It provides an interface between wireless Art-Net and wired DMX512.
*/

#include <ESP8266WiFi.h>         // https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <ArtnetWifi.h>          // https://github.com/rstephan/ArtnetWifi

#include <Time.h>
#include <TimeLib.h>

#include <uptime.h>

#include "webserverAndOta.h"
#include "send_break.h"

#include <dmxmapping.h>

#define MIN(x,y) (x<y ? x : y)
#define ENABLE_MDNS
#define ENABLE_WEBINTERFACE
// #define COMMON_ANODE

Config config;
ESP8266WebServer server(80);
const char* host = "ARTNET";

const char* version = __DATE__ " / " __TIME__;

// ALL LED Pins on one site of wemos d1 mini
#define LED_R 14  // GPIO14/D5
#define LED_G 12  // GPIO12/D6
#define LED_B 13  // GPIO13/D7

#ifdef COMMON_ANODE
#define ON  0
#define OFF 1
#else
#define ON  1
#define OFF 0
#endif

// Artnet settings
ArtnetWifi artnet;
unsigned long packetCounter = 0, 
              seqErrCounter = 0,
              frameCounter = 0;
float fps = 0;

// Global universe buffer
struct
{
  uint16_t universe;
  uint16_t length;
  uint8_t sequence;
  uint8_t data[513];
} global;

// keep track of the timing of the function calls
uint64_t tic_loop = 0, tic_fps = 0, tic_packet = 0, tic_web = 0, tic_dmx = 0 ;

// LEDs
void LedBlack()
{
  digitalWrite(LED_R, OFF);
  digitalWrite(LED_G, OFF);
  digitalWrite(LED_B, OFF);
}

void LedRed()
{
  digitalWrite(LED_R, ON);
  digitalWrite(LED_G, OFF);
  digitalWrite(LED_B, OFF);
}

void LedGreen()
{
  digitalWrite(LED_R, OFF);
  digitalWrite(LED_G, ON);
  digitalWrite(LED_B, OFF);
}

void LedBlue()
{
  digitalWrite(LED_R, OFF); 
  digitalWrite(LED_G, OFF);
  digitalWrite(LED_B, ON);
}

void LedYellow()
{
  digitalWrite(LED_R, ON);
  digitalWrite(LED_G, ON);
  digitalWrite(LED_B, OFF);
}

void LedMagenta()
{
  digitalWrite(LED_R, ON);
  digitalWrite(LED_G, OFF);
  digitalWrite(LED_B, ON);
}

void LedCyan()
{
  digitalWrite(LED_R, OFF);
  digitalWrite(LED_G, ON);
  digitalWrite(LED_B, ON);
}

//this will be called for each UDP packet received
void onDmxPacket(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t * data)
{
  if (universe != config.universe)
  {
    // wrong universe -> cancel here
    // falsches Universum -> abbrechen
    return ;
  }

  if (sequence > global.sequence ||
      sequence == 0 ||
      sequence + 240 < global.sequence)
  {
    // Sequence checked:
    // sequence > last sequence OR
    // sequence = 0 () "Sequence field is set to 0x00 to disable this feature"
    // sequence much smaller than last sequence (~ 240) -> new beginning 
    //               (hope not to lose more than 15 packets)
    // once out of sync might last 6 seconds to sync again
    global.sequence = sequence ;
  }
  else
  {
    seqErrCounter++;

  #ifdef VERBOSE
    Serial.println(String("Wrong Seq <") + sequence + "> after <" + global.sequence + ">");
  #endif
    return ;
  }

  // print some feedback
  packetCounter++ ;
  tic_packet = millis();

  //Serial.print("pkt=");
  //Serial.print(packetCounter++);
  if ((millis() - tic_fps) > 1000 ||
       frameCounter > 100)
  {
    // don't calculate fps too often
    fps = 1000 * frameCounter / (millis() - tic_fps);
    tic_fps = millis();
    frameCounter = 0;
#ifdef VERBOSE
    Serial.print("pkt=");
    Serial.print(packetCounter);
    Serial.print(", SeqErr=");
    Serial.print(seqErrCounter);
    Serial.print(", FPS=");
    Serial.print(fps);
    Serial.println();
#endif
  }

  if (universe == config.universe)
  {
    // copy the data from the UDP packet over to the global universe buffer
    global.universe = universe;
    global.sequence = sequence;

#if DMX_DEBUG
    if (frameCounter == 0)
    {
      Serial.print("UNI: ");
      Serial.print(universe);
      Serial.print(" - ");
    }
#endif

#ifdef MAPS
    if (config.useMaps ||
        ((config.mapChan > 0) && (data[config.mapChan -1] > 127)))
    {
      dmxMapsExec(data, length);
    }
#endif

    if (length < 512)
      global.length = length;
    for (int i = 0; i < global.length; i++)
    {
      global.data[i] = data[i];

#if DMX_DEBUG
      // DEBUG
      if (frameCounter == 0 &&
          i < 8)
      {
        Serial.print(global.data[i]);
        Serial.print(" ");
      }
#endif
    }
#if DMX_DEBUG
    if (frameCounter == 0)
      Serial.println("");
#endif
  }
} // onDmxpacket

void setup()
{
  // leds off on start
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  LedBlack() ;

  Serial1.begin(250000, SERIAL_8N2);
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

#ifdef VERBOSE
  Serial.println("");
  Serial.println("ESP Artnet DMX");
#ifdef MAPS
  Serial.println(" [Mapping]");
#endif
  Serial.println("setup starting");
#endif


  global.universe = 0;
  global.sequence = 0;
  global.length = 512;
  //global.data = (uint8_t *)malloc(512);
  //for (int i = 0; i < 512; i++)
  //  global.data[i] = 0;
  memset(global.data, 0, sizeof(global.data));  // simplify 

#ifdef LITTLEFS
  LittleFS.begin();
#else
  SPIFFS.begin();
#endif

#if MAPS
  dmxMapsInit() ;
  chan2DmxMapsRead() ;
  dmxMapsRead();
#endif

  initialConfig();

  if (loadConfig())
  {
    LedYellow();
  }
  else
  {
    LedRed();
  }
  delay(1000);

  if (WiFi.status() != WL_CONNECTED)
    LedRed();

  WiFiManager wifiManager;
  // wifiManager.resetSettings();
  WiFi.hostname(host);
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  wifiManager.autoConnect(host);
#ifdef VERBOSE
  Serial.print(host);
  Serial.println(" connected");
#endif

  if (WiFi.status() == WL_CONNECTED)
    LedGreen();

#ifdef ENABLE_WEBINTERFACE
  // this serves all URIs that can be resolved to a file on the SPIFFS filesystem
  server.onNotFound(handleNotFound);

  server.on("/", HTTP_GET, []() {
    tic_web = millis();
    handleRedirect("/index");
  });

  server.on("/index", HTTP_GET, []() {
    tic_web = millis();
    handleStaticFile("/index.html");
  });

  server.on("/defaults", HTTP_GET, []() {
    tic_web = millis();
#ifdef VERBOSE
    Serial.println("handleDefaults");
#endif
    handleStaticFile("/success.html");
    delay(2000);
    LedRed();
    initialConfig();
    saveConfig();
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    WiFi.hostname(host);

    finalizeWebServer(2000) ;
    ESP.restart();
  });

  server.on("/reconnect", HTTP_GET, []() {
    tic_web = millis();
#ifdef VERBOSE
    Serial.println("handleReconnect");
#endif
    handleStaticFile("/success.html");
    delay(2000);
    LedRed();
    WiFiManager wifiManager;
    wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.startConfigPortal(host);
  #ifdef VERBOSE
    Serial.println("connected");
  #endif
    if (WiFi.status() == WL_CONNECTED)
      LedGreen();
  });

  server.on("/reset", HTTP_GET, []() {
    tic_web = millis();
#ifdef VERBOSE
    Serial.println("handleReset");
#endif
    handleStaticFile("/success.html");
    LedRed();
    finalizeWebServer(2000) ;
    ESP.restart();
  });

  server.on("/monitor", HTTP_GET, [] {
    tic_web = millis();
    handleStaticFile("/monitor.html");
  });

  server.on("/settings", HTTP_GET, [] {
    tic_web = millis();
    handleStaticFile("/settings.html");
  });

  server.on("/dir", HTTP_GET, [] {
    tic_web = millis();
    handleDirList(false);
  });

  server.on("/mapdir", HTTP_GET, [] {
    tic_web = millis();
    handleDirList(true);
  });

  server.on("/json", HTTP_PUT, [] {
    tic_web = millis();
    handleJSON();
  });

  server.on("/json", HTTP_POST, [] {
    tic_web = millis();
    handleJSON();
  });

  server.on("/json", HTTP_GET, [] {
#ifdef VERBOSE
  Serial.println("json GET") ;
#endif
    char cTime[16] ;
    tic_web = millis();
    DynamicJsonDocument root(300);
    
    // Konfigurationswerte
    CONFIG_TO_JSON(universe, "universe");
    CONFIG_TO_JSON(channels, "channels");
    CONFIG_TO_JSON(delay, "delay");
    CONFIG_TO_JSON(useMaps, "useMaps");
    CONFIG_TO_JSON(mapChan, "mapChan");

    // Infos
    root["version"] = version;
    // readable time
    //root["uptime"]  = long(millis() / 1000);
    uptime::calculateUptime() ;
    snprintf(cTime, sizeof(cTime), "%03d:%02d:%02d", 
            ((int)uptime::getDays() * 24) + (int)uptime::getHours(), 
            (int)uptime::getMinutes(), 
            (int)uptime::getSeconds());
    root["uptime"] = String(cTime);
    root["packets"] = packetCounter;
    root["seqErr"] = String(seqErrCounter) + 
                     String(" - ") + 
                     String(round(((double)((seqErrCounter * 10000) / ((packetCounter != 0) ? packetCounter : 1)))) / 100) +
                     String(" %") ;
    root["fps"]     = fps;
    String str;
    serializeJson(root, str);
    server.send(200, "application/json", str);
  });


  server.on("/upload", HTTP_GET, []() {
    tic_web = millis();
    handleStaticFile("/upload.html"); 
  });

  server.on("/upload", HTTP_POST, handleMapUploadFn, handleMapUploadUpload);


  server.on("/update", HTTP_GET, [] {
    tic_web = millis();
    handleStaticFile("/update.html");
  });

  server.on("/update", HTTP_POST, handleFwUpdateFn, handleFwUpdateUpload);

  // start the web server
  server.begin();
#endif

  // announce the hostname and web server through zeroconf
#ifdef ENABLE_MDNS
  MDNS.begin(host);
  MDNS.addService("http", "tcp", 80);
#endif

  artnet.begin();
  artnet.setArtDmxCallback(onDmxPacket);

  // initialize all timers
  tic_loop   = millis();
  tic_packet = millis();
  tic_fps    = millis();
  tic_web    = 0;
  tic_dmx    = millis();

#ifdef VERBOSE
  Serial.println("setup done");

  Serial.print("Universe: ");
  Serial.print(config.universe);
  Serial.print(" - ");
  Serial.println(global.universe); 
  Serial.print("Channels: ");
  Serial.println(config.channels);
  Serial.print("Delay: ");
  Serial.println(config.delay);
#endif
} // setup

void loop()
{
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED)
  {
    LedRed();
    //delay(10);
    yield() ;
  }
  else if ((millis() - tic_web) < 2000)
  {
    LedBlue();
    //delay(25);
    yield() ;
  }
  else
  {
    artnet.read();

    tic_loop = millis() ;

    if (tic_loop - tic_packet < 1000)
    {
      // last packet receive within one second -> green
      LedGreen();
    }
    else
    {
      // last packet receive more than one second ago -> yellow
      LedMagenta();
    }

    // this section gets executed at a maximum rate of around 40Hz (depending on config.delay)
    if ((tic_loop - tic_dmx) > config.delay)
    {
      tic_dmx = millis();
      frameCounter++;

      sendBreak();

      Serial1.write(0); // Start-Byte
      // send out the value of the selected channels (up to 512)
      for (int i = 0; i < MIN(global.length, config.channels); i++)
      {
        Serial1.write(global.data[i]);
      }
    }
  }

  //delay(1);
  yield() ;
} // loop

