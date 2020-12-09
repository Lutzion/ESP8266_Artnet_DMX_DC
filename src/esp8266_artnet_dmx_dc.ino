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

#ifdef PERS_WIFI
#include <PersWiFiManager.h>
#include <DNSServer.h>
#else
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#endif
#include <WiFiClient.h>
#include <ArtnetWifi.h>          // https://github.com/rstephan/ArtnetWifi

//#ifdef WEBSOCKET
#include <WebSocketsServer.h>
//#endif

#include <Time.h>
#include <TimeLib.h>

#include <uptime.h>

#include "webserverAndOta.h"
#include "send_break.h"
#include "esp8266_artnet_dmx_dc.h"

#include <dmxmapping.h>

#ifdef MRD_ESP8266_RTC 
#include <MultiResetDetector.h>

#define RESET_COUNT_WM_ALLOW 2  // Anzahl der Resets, bevor der WifiManager erlaubt wird
#define RESET_COUNT_WM_FORCE 5  // Anzahl der Resets, bevor der WifiManager erzwungen wird

volatile int resetCount = 0;
#endif 

#define MIN(x,y) (x<y ? x : y)
#define ENABLE_MDNS
#define ENABLE_WEBINTERFACE
// #define COMMON_ANODE

//#define CONNECT_TIMEOUT 60000   /* 60 seconds */

Config config;
ESP8266WebServer server(80);
#ifdef WEBSOCKET
WebSocketsServer webSocket = WebSocketsServer(443);
#endif

#ifdef PERS_WIFI 
DNSServer dnsServer;
PersWiFiManager persWM(server, dnsServer);
bool bToggleAttempt ;
#endif

#ifdef USE_E131
// ESPAsyncE131 instance with UNIVERSE_COUNT buffer slots
ESPAsyncE131 e131(1);
uint8_t   e131SeqTracker ;
uint8_t   e131SeqError ;
#endif

const char* host = "ARTNET_DC" ;

//const char* version = __DATE__ " / " __TIME__;

// ALL LED Pins on one site of wemos d1 mini
#define LED_R 14  // GPIO14/D5
#define LED_G 12  // GPIO12/D6
#define LED_B 13  // GPIO13/D7

#ifdef COMMON_ANODE
const uint8_t ON = 0 ;
const uint8_t OFF = 1 ;
#else
const uint8_t ON = 1 ;
const uint8_t OFF = 0 ;
#endif

// Artnet settings
#define ARTNET_PORT 0x1936
#define ARTNET_REPLY_SIZE 239

#if POLL_REPLY
ArtnetWifiDC artnet;
artnetPollReply apr ;
#else
ArtnetWifi artnet;
#endif
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
uint64_t tic_loop = 0, 
         tic_fps = 0, 
         tic_packet = 0, 
         tic_web = 0, 
         tic_dmx = 0, 
         tic_webSocket = 0,
         tic_artnetPollReply = 0 ;

uint64_t aprTime;

char version[64];

char* __DATE__2Date(char* sversion)
{
  const char date[]=__DATE__;
  const char time[]=__TIME__;
  char cMonth[4];
  const char names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  int year, month, day, hour, min, sec;

  if (strlen(date) != 11 || 
      strlen(time) != 8)
    return NULL;

  sscanf(date, "%s %d %d", cMonth, &day, &year);
  sscanf(time, "%d:%d:%d", &hour, &min, &sec);
  month = (strstr(names, cMonth) - names) / 3 + 1;
  sprintf(sversion,"%04d.%02d.%02d-%02d:%02d:%02d", year, month, day, hour, min, sec);
  return sversion;
}

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

void LedWhite()
{
  digitalWrite(LED_R, ON);
  digitalWrite(LED_G, ON);
  digitalWrite(LED_B, ON);
}

void ShowReset()
{
  LedWhite() ;
  delay(100);
  LedBlack() ;
}

String reverseString(String strIn)
{
  String strOut ;

  int len = strIn.length() - 1;
 
  for(int i = len; i >= 0; i--)
  {
    strOut = strOut + strIn[i] ;
  }

  return strOut ;
}

#ifdef WEBSOCKET
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  switch(type)
  {
  case WStype_ERROR:
    Serial.print("WStype_ERROR <");
    Serial.print(num) ;
    Serial.println(">");
    break;
  case WStype_DISCONNECTED:
    Serial.print("WStype_DISCONNECTED <");
    Serial.print(num) ;
    Serial.println(">");
    webSocket.begin();
    break;
  case WStype_CONNECTED:
    Serial.print("WStype_CONNECTED <");
    Serial.print(num) ;
    Serial.print("-");
    Serial.print(webSocket.remoteIP(num)) ;
    Serial.println(">");
    break;
  /*
  case WStype_TEXT:
    Serial.print("WStype_TEXT <");
    {
      // EMPFANG VON HTML
      for(unsigned int i = 0; i < length; i++) 
        Serial.print((char) payload[i]);
      Serial.println(">");
    }
    break;
  */
  default:
    Serial.print("webSocketEvent <");
    Serial.print(type);
    Serial.println(">");
  }
}
#endif

#if POLL_REPLY
void artnetPollReplyInit()
{
  memset(&apr, 0, sizeof(apr));
  snprintf((char*)apr.Artnet, sizeof(apr.Artnet), "Art-Net") ; 
  apr.OpCodeLo = (unsigned char)0 ;
  apr.OpCodeHi = (unsigned char)0x21 ;
  // IPAdress changeable -> dynamic entry in ArtNetPolled()
  apr.PortNumberLo = 0x36 ;
  apr.PortNumberHi = 0x19 ;
  //NetSwitch ; -> Subnet -> dynamic entry
  //SubSwitch ;
  apr.Status1 = 0b11100000 ;  // 11100000 128+64+32 = 
  apr.EstaManLo = 0 ; // https://tsp.esta.org/tsp/working_groups/CP/mfctrIDs.php
  apr.EstaManHi = 0 ;
  snprintf((char*)apr.ShortName, sizeof(apr.ShortName), "Artnet-DMX-DC") ; 
  snprintf((char*)apr.LongName, sizeof(apr.LongName), "Artnet-DMX-DC by lutZion") ; 
  apr.NumPortsLo = 1 ;
  apr.PortTypes[0] = 0x80 ; // Bit 7 -> output implemented
  apr.GoodOutput[0] = 0x80 ; // Bit 7 -> Data transmitted
  //apr.Mac -> dynamic entry
  //apr.Status2 -> dynamic entry
}
#endif


void DmxStatistics()
{
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
    Serial.print("ARTNET pkt=");
    Serial.print(packetCounter);
    Serial.print(", SeqErr=");
    Serial.print(seqErrCounter);
    Serial.print(", FPS=");
    Serial.print(fps);
    Serial.println();
#endif

#ifdef WEBSOCKET
    // like in server.on("/json", HTTP_GET, []
    char cTime[16] ;
    uptime::calculateUptime() ;
    snprintf(cTime, sizeof(cTime), "%03d:%02d:%02d", 
            ((int)uptime::getDays() * 24) + (int)uptime::getHours(), 
            (int)uptime::getMinutes(), 
            (int)uptime::getSeconds());

    String bufSnd = String (cTime) + "#" +
                    String(packetCounter) + "#" +
                    String(seqErrCounter) + 
                    String(" - ") + 
                    String(round(((double)((seqErrCounter * 10000) / ((packetCounter != 0) ? packetCounter : 1)))) / 100) +
                    String(" %") + "#" +
                    String(fps) + "#";

    webSocket.broadcastTXT(bufSnd);
#endif
  }
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

  DmxStatistics() ;

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


#if POLL_REPLY
void ArtNetPolled()
{
  #ifdef VERBOSE
    Serial.println("ArtNetPolled");
  #endif
 
  IPAddress ip = WiFi.localIP();
  memcpy(apr.IpAdress, (unsigned char*)&ip.v4(), 4) ;

  IPAddress subnet = WiFi.subnetMask();
  IPAddress broadCast =  {(uint8_t)(~subnet[0] | (ip[0] & subnet[0])), 
                          (uint8_t)(~subnet[1] | (ip[1] & subnet[1])), 
                          (uint8_t)(~subnet[2] | (ip[2] & subnet[2])), 
                          (uint8_t)(~subnet[3] | (ip[3] & subnet[3]))};

  // https://github.com/mtongnz/ESP8266_ArtNetNode_DMX/issues/6  -> .v4()
#ifdef SDK1
  apr.NetSwitch = highByte(subnet+1);  //subnet hi-lo
  apr.SubSwitch = lowByte(subnet+1);
#else
  /*
  apr.NetSwitch = highByte(subnet.v4()+1);  //subnet hi-lo
  apr.SubSwitch = lowByte(subnet.v4()+1);
  */
  apr.NetSwitch = highByte((uint32_t)subnet+1);  //subnet hi-lo
  apr.SubSwitch = lowByte((uint32_t)subnet+1);
#endif
  apr.SwOut[0] = config.universe + 16 * 0 ;

  uint8_t MAC_array[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(MAC_array);

  for (int i = 0; i < 6; i++)
  {
    apr.Mac[i] = MAC_array[i];
  }

  apr.Status2 = (/*dhcp*/ 1) ? 15 : 13;

  if (sizeof(apr) != ARTNET_REPLY_SIZE)
  {
    Serial.println("ArtNetPolled sizeof(apr) != ARTNET_REPLY_SIZE");
    return ;
  }

  // Send packet
  artnet.UDP().beginPacket(broadCast, ARTNET_PORT);
  artnet.UDP().write((char *)&apr, ARTNET_REPLY_SIZE);
  artnet.UDP().endPacket();
}
#endif

#ifdef PERS_WIFI 
PersWiFiManager::WiFiChangeHandlerFunction wifiConnectionAttempt()
{
  bToggleAttempt = !bToggleAttempt ;
  if (bToggleAttempt)
  {
    LedRed() ;
    Serial.print(".");
  }
  else
  {
    LedBlack() ;
  }

  return 0 ;
}
#endif

void setup()
{
  // leds off on start -> color cyan because of default states
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  LedBlack() ;

  randomSeed(analogRead(0));

  __DATE__2Date(version);

  Serial1.begin(250000, SERIAL_8N2);
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

#ifdef MRD_ESP8266_RTC 
  resetCount = MultiResetDetector::execute(1000, 0, ShowReset) ;
#ifdef VERBOSE
  if (resetCount > 1)
  {
    Serial.println("") ;
    Serial.print("resetCount = ");
    Serial.println(resetCount) ;
  }
#endif
#endif

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

  // === 
  char chipId[64] = { 0 };
  snprintf(chipId, sizeof(chipId), "%08X", ESP.getChipId());
  char hostMitId[32] = { 0 };
  snprintf(hostMitId, sizeof(hostMitId), "%s_%s", host, chipId);

  // chipId umkehren
  String strPsk = String(chipId) ;
  strPsk = reverseString(strPsk) ;

  sprintf(chipId, strPsk.c_str()) ;
  if (strlen(config.psk) > 0)
  {
    strcpy(chipId, config.psk) ; // min(sizeof(chipId), strlen(config.psk) + 1));
  }

#ifdef PERS_WIFI
  //if (resetCount >= RESET_COUNT_WM_ALLOW)
  if (true)
  {
    String ssid = hostMitId ;
    String psk  = chipId;

    LedRed() ;
    persWM.setApCredentials(ssid, psk);
    persWM.setTimeout(config.startDelay);
    persWM.onAttempt(wifiConnectionAttempt);
    persWM.begin();
  }
#else
  uint32_t timeout = millis();

  if (resetCount >= RESET_COUNT_WM_FORCE)
  {
    // force WiFiManager -> for example for changing existing settings that work
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    WiFi.hostname(hostMitId);
    wifiManager.autoConnect(hostMitId, chipId);
  }
  else
  {
    if (resetCount < RESET_COUNT_WM_ALLOW)
    {
      WiFi.begin();
    }
  } 

  while (WiFi.status() != WL_CONNECTED) 
  {
    LedRed() ;
    delay(500);
    LedBlack() ;
    delay(500);

    if (Serial)
    {
      uint32_t secs = (millis() - timeout) / 1000 ;
      if (secs % 10 != 0)
        Serial.print(".");
      else
        Serial.print(secs);
    }
    if (millis() - timeout > (config.startDelay * 1000)) // CONNECT_TIMEOUT) 
    {
      if (Serial) 
      {
          Serial.println(F(" not connect after timeout"));
      }
      break;
    }
  }
#endif

  if (WiFi.status() == WL_CONNECTED)
    LedGreen();
  else
    LedRed() ;

#ifndef PERS_WIFI
  if (resetCount >= RESET_COUNT_WM_ALLOW /*&&
      resetCount < RESET_COUNT_WM_FORCE*/)
  {
    // allow WiFiManager 
    WiFiManager wifiManager;
    // wifiManager.resetSettings();
    WiFi.hostname(hostMitId);
    //wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.autoConnect(hostMitId, chipId);
  }
#endif

#ifdef VERBOSE
  Serial.print(hostMitId);
//  Serial.print(" - ");
//  Serial.print(chipId);
  Serial.println("");
#endif

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

#ifdef PERS_WIFI
  server.on("/wifi.htm", HTTP_GET, []()
  {
    tic_web = millis();
#ifdef MRD_ESP8266_RTC
    if (resetCount >= RESET_COUNT_WM_ALLOW)
    {
      handleStaticFile("/wifi.htm");
    }
    else
    {
      handleStaticFile("/failure.html");
      delay(1000);
    }
#else
    handleStaticFile("/wifi.htm");
#endif
  });
#endif

  server.on("/defaults", HTTP_GET, []() {
#ifdef MRD_ESP8266_RTC
    if (resetCount < RESET_COUNT_WM_ALLOW)
    {
      handleStaticFile("/failure.html");
      delay(1000);
      return ;
    }
#endif

    tic_web = millis();
#ifdef VERBOSE
    Serial.println("handleDefaults");
#endif
    handleStaticFile("/success.html");
    delay(2000);
    LedRed();
    initialConfig();
    saveConfig();
#ifndef PERS_WIFI
    WiFiManager wifiManager;
    wifiManager.resetSettings();
#endif
    WiFi.hostname(host);

    finalizeWebServer(2000) ;
    ESP.restart();
  });

#ifndef PERS_WIFI
  server.on("/reconnect", HTTP_GET, []() {
    tic_web = millis();
#ifdef VERBOSE
    Serial.println("handleReconnect");
#endif
    handleStaticFile("/success.html");
    delay(2000);
    LedRed();
    WiFiManager wifiManager;
    //wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.startConfigPortal(host);
  #ifdef VERBOSE
    Serial.println("connected");
  #endif
    if (WiFi.status() == WL_CONNECTED)
      LedGreen();
  });
#endif

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

  server.on("/dir", HTTP_GET, [] 
  {
    tic_web = millis();
    String argFile = "" ;
    if (server.hasArg("delete"))
    {
      argFile = server.arg("delete") ;
    }
    handleDirList(false, argFile);
  });

  server.on("/mapdir", HTTP_GET, []
  {
    tic_web = millis();

    /*
    for (int i = 0; i < server.headers(); i++)
    {
      Serial.println("Head " + (String)i + " –> " + 
                               server.headerName(i) + ": " + 
                               server.header(i)) ;
    }
    */
    /*
    for (int i = 0; i < server.args(); i++)
    {
      Serial.println("Arg " + (String)i + " –> " + 
                              server.argName(i) + ": " + 
                              server.arg(i)) ;
    }
    */
    String argFile = "" ;
    if (server.hasArg("delete"))
    {
      argFile = server.arg("delete") ;
    }
    handleDirList(true, argFile);
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
    CONFIG_TO_JSON(startDelay, "startDelay");
    CONFIG_TO_JSON(e131Start, "e131Start");
    CONFIG_TO_JSON(e131Unicast, "e131Unicast");
    S_CONFIG_TO_JSON(psk, "psk");

    // Infos
    root["version"] = String(version) + 
                      ((resetCount != 0) ? (" [" + String(resetCount) + "]") : "");
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

#ifdef WEBSOCKET
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
#endif
#endif

  // announce the hostname and web server through zeroconf
#ifdef ENABLE_MDNS
  MDNS.begin(host);
  MDNS.addService("http", "tcp", 80);
#endif

  artnet.begin();
  artnet.setArtDmxCallback(onDmxPacket);

#ifdef POLL_REPLY
  artnetPollReplyInit() ;
  ArtNetPolled() ;
#endif

#ifdef USE_E131
  if (config.e131Start)
  {
    if (!config.e131Unicast)
      e131.begin(E131_MULTICAST, config.universe + 1);
    else
      e131.begin(E131_UNICAST, config.universe + 1);
  }
#endif

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
#ifdef PERS_WIFI
  dnsServer.processNextRequest();
#endif

  server.handleClient();
#ifdef WEBSOCKET
  webSocket.loop();
#endif
 
  // Wenn nicht im AP-Mode muss CONNECTED sein
  // Wenn im AP-Mode ist es WL_DISCONNECTED
  // Wenn beide Bedingungen nicht erfuellt -> Fehler
  if (!(((WiFi.getMode() != WIFI_AP) && (WiFi.status() == WL_CONNECTED)) ||
        (((WiFi.getMode() == WIFI_AP) && (WiFi.status() == WL_DISCONNECTED)))))
  {
    LedRed();
    //delay(10);
    yield() ;
    /*
    Serial.print("Wifi.S: ");
    Serial.print(WiFi.status());
    Serial.print(" M: ");
    Serial.println(WiFi.getMode());
    */
  }
  else if ((millis() - tic_web) < 2000)
  {
    LedBlue();
    //delay(25);
    yield() ;
  }
  else
  {
    tic_loop = millis() ;

    uint16_t opCode = artnet.read();

#if POLL_REPLY
    if (opCode == ART_POLL)
    {
      // TODO: Random delay max. 3 seconds -> 
      // https://art-net.org.uk/structure/discovery-packets/artpollreply/

      // DO NOT DELAY WITH delay() as it blocks program
      // set timer for later
      aprTime = random(0, 1000);

      tic_artnetPollReply = tic_loop + aprTime ;
      //ArtNetPolled();
    }

    if (tic_artnetPollReply != 0 &&
        tic_artnetPollReply < tic_loop)
    {
      tic_artnetPollReply = 0 ;
      ArtNetPolled();
    }
#endif

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


#ifdef USE_E131

    if (config.e131Start)
    {
      if (!e131.isEmpty())
      {
          e131_packet_t packet;
          e131.pull(&packet);     // Pull packet from ring buffer

#ifdef E131_VERBOSE          
          Serial.printf("E131: Uni %u / %u Chans | Packet: %u / Seq: %u / Err: %u / %02X %02X\n",
                  htons(packet.universe),                 // The Universe for this packet
                  htons(packet.property_value_count) - 1, // Start code is ignored, we're interested in dimmer data
                  e131.stats.num_packets,                 // Packet counter
                  packet.sequence_number,
                  e131.stats.packet_errors,               // Packet error counter
                  packet.property_values[1],              // Dimmer data for Channel 1
                  packet.property_values[2]);             // Dimmer data for Channel 2
#endif 

        uint16_t uni = htons(packet.universe) ;
        uint16_t count = htons(packet.property_value_count) - 1 ;

        if (uni == (config.universe + 1))
        {
          // Universe offset and sequence tracking
          //uint8_t uniOffset = (e131.universe - config.universe);
          e131SeqTracker++;
          if (packet.sequence_number != e131SeqTracker)
          {
              e131SeqError++;
              e131SeqTracker = packet.sequence_number ;
              // Sequenzfehler -> Abbruch

              seqErrCounter++;
          }
          else
          {
                // Daten uebernehmen
#ifdef MAPS
            if (config.useMaps ||
                ((config.mapChan > 0) && (packet.property_values[config.mapChan] > 127)))
            {
              dmxMapsExec(&packet.property_values[1], count);
            }
#endif
            if (count <= 512)
              global.length = count;
            for (int i = 0; i < global.length; i++)
            {
              global.data[i] = packet.property_values[i+1];
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

            packetCounter++ ;
            tic_packet = millis();

            DmxStatistics() ;
          }
        } // if (packet.universe == (config.universe + 1))
      } // if (!e131.isEmpty())
    } // if (config.e131Start)

#endif


#ifdef WEBSOCKET
    if (tic_loop - tic_webSocket >= 1000)
    {
      tic_webSocket = tic_loop ;

      // like in server.on("/json", HTTP_GET, []
      char cTime[16] ;
      uptime::calculateUptime() ;
      snprintf(cTime, sizeof(cTime), "%03d:%02d:%02d", 
              ((int)uptime::getDays() * 24) + (int)uptime::getHours(), 
              (int)uptime::getMinutes(), 
              (int)uptime::getSeconds());

      String bufSnd = String (cTime) ;/*+ "#" +
                      String(packetCounter) + "#" +
                      String(seqErrCounter) + 
                      String(" - ") + 
                      String(round(((double)((seqErrCounter * 10000) / ((packetCounter != 0) ? packetCounter : 1)))) / 100) +
                      String(" %") + "#" +
                      String(fps) + "#";*/

      webSocket.broadcastTXT(bufSnd);
    }
#endif

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

