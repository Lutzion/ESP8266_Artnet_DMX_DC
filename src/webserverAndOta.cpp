/*
 Original by
 https://github.com/robertoostenveld/arduino/tree/master/esp8266_artnet_dmx512
 Changes by
 https://github.com/Lutzion/
*/

#include "webserverAndOta.h"

#include <list>

extern ESP8266WebServer server;
extern Config config;
extern unsigned long packetCounter;

File fsUploadFile;              // a File object to temporarily store the received file

std::list<file> lstFile ;

/***************************************************************************/

static String getContentType(const String& path) {
  if (path.endsWith(".html"))       return "text/html";
  else if (path.endsWith(".htm"))   return "text/html";
  else if (path.endsWith(".css"))   return "text/css";
  else if (path.endsWith(".txt"))   return "text/plain";
  else if (path.endsWith(".js"))    return "application/javascript";
  else if (path.endsWith(".png"))   return "image/png";
  else if (path.endsWith(".gif"))   return "image/gif";
  else if (path.endsWith(".jpg"))   return "image/jpeg";
  else if (path.endsWith(".jpeg"))  return "image/jpeg";
  else if (path.endsWith(".ico"))   return "image/x-icon";
  else if (path.endsWith(".svg"))   return "image/svg+xml";
  else if (path.endsWith(".xml"))   return "text/xml";
  else if (path.endsWith(".pdf"))   return "application/pdf";
  else if (path.endsWith(".zip"))   return "application/zip";
  else if (path.endsWith(".gz"))    return "application/x-gzip";
  else if (path.endsWith(".json"))  return "application/json";
  else if (path.endsWith(".map"))   return "text/plain";  // for dmx-mapping
  return "application/octet-stream";
}

/***************************************************************************/

// configuration functions
bool initialConfig()
{
  config.universe = 0;
  config.channels = 512;
  config.delay = 25;
  config.useMaps = 0 ;
  config.mapChan = 0 ;
  return true;
}

bool loadConfig()
{
#ifdef VERBOSE
  Serial.println("loadConfig");
#endif

#ifdef LITTLEFS
  File configFile = LittleFS.open("/config.json", "r");
#else
  File configFile = SPIFFS.open("/config.json", "r");
#endif
  if (!configFile)
  {
#ifdef VERBOSE
    Serial.println("Failed to open config file");
#endif
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
#ifdef VERBOSE
    Serial.println("Config file size is too large");
#endif
    return false;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  StaticJsonDocument<300> jsonBuffer;
  DynamicJsonDocument root(1024);
  DeserializationError error = deserializeJson(root, buf.get());
  if (error){
#ifdef VERBOSE
    Serial.println("Failed to parse config file");
#endif
    return false;
  }

  JSON_TO_CONFIG(universe, "universe");
  JSON_TO_CONFIG(channels, "channels");
  JSON_TO_CONFIG(delay, "delay");
  JSON_TO_CONFIG(useMaps, "useMaps");
  JSON_TO_CONFIG(mapChan, "mapChan");

  return true;
}

bool saveConfig()
{
#ifdef VERBOSE
  Serial.println("saveConfig");
#endif
  DynamicJsonDocument root(300);

  CONFIG_TO_JSON(universe, "universe");
  CONFIG_TO_JSON(channels, "channels");
  CONFIG_TO_JSON(delay, "delay");
  CONFIG_TO_JSON(useMaps, "useMaps");
  CONFIG_TO_JSON(mapChan, "mapChan");

#ifdef LITTLEFS
  File configFile = LittleFS.open("/config.json", "w");
#else
  File configFile = SPIFFS.open("/config.json", "w");
#endif
  if (!configFile)
  {
#ifdef VERBOSE
    Serial.println("Failed to open config file for writing");
#endif
    return false;
  }
  else {
#ifdef VERBOSE
    Serial.println("Writing to config file");
#endif
    serializeJson(root, configFile);
    configFile.close();
    return true;
  }
}

/***************************************************************************/

// === finalizeWebServer()
// do the outstanding web server tasks
void finalizeWebServer(uint16_t iDelay)
{
  for(uint16_t i = 0; i < (iDelay / 100); i++)
  {
    server.handleClient();
    delay(100);
  }
}

// Functions for (map)file uploads
void handleMapUploadFn()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  //server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  server.sendHeader("Location", (Update.hasError()) ? "/failure.html" : "/success.html");
  server.send(303);
}

void handleMapUploadUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);

#ifdef LITTLEFS
    fsUploadFile = LittleFS.open(filename, "w");
#else
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
#endif

    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); 
      Serial.println(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
    } else {
      //server.send(500, "text/plain", "500: couldn't create file");
      server.sendHeader("Location","/failure.html");      // Redirect the client to the failure page
      server.send(303);
    }
  }
}

// Functions for firmware updates
void handleFwUpdateFn()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  //server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  // Redirect the client to the failure page
  server.sendHeader("Location", (Update.hasError()) ? "/failure.html" : "/success.html");
  server.send(303);

  finalizeWebServer(2000) ;
  ESP.restart();
}

void handleFwUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.setDebugOutput(true);
    WiFiUDP::stopAll();
    Serial.printf("Update: %s\n", upload.filename.c_str());
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) { //start with max available size
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { //true to set the size to the current progress
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);
  }
  yield();
}

// compareFiles()
// sort files by name
bool compareFiles(file& first, file& second)
{
  String f = first.name ;
  String s = second.name ;
  f.toLowerCase() ;
  s.toLowerCase() ;

  return ( f.compareTo(s) < 0 ) ;
}

// handleDirList()
// if mapOnly = true -> show only files related to dmxmapping
// files to list, sort list, print list, clear list
void handleDirList(bool mapOnly)
{
#ifdef VERBOSE
  Serial.println("handleDirList");
#endif

  lstFile = std::list<file>();

  String str = "<!doctype html>"
               "<html>"
               "<head>"
                "<title>ESP Artnet DMX - Directory</title>"
                "<meta charset='utf-8'>"
                "<link href='style.css' rel='stylesheet' type='text/css'>"
                "</head>"
                "<body>"
                "<div class='content'>"
                "<h1>Directory <a href='/'>&lt;&lt;&lt;</a></h1>"

                "<table align='center'>"
                "<tr><th>Name</th><th>[bytes]</th></tr>";

#ifdef LITTLEFS
  Dir dir = LittleFS.openDir("/");
#else
  Dir dir = SPIFFS.openDir("/");
#endif

  while (dir.next())
  {
    String sfile = dir.fileName() ;
    sfile.toUpperCase() ;
    if (!mapOnly ||
        sfile.indexOf("MAP") != -1)
    {
      // not mapOnly -> show all, 
      // mapOnly and MAP in filename -> show dmxmapping related files
      file f = file() ;
      f.name = dir.fileName() ;
      f.size = dir.fileSize();
      f.time = dir.fileTime();
      f.creationTime = dir.fileCreationTime();
      lstFile.push_back(f) ;
      /*
      str = str + "<tr><td align='left'><a href=" + dir.fileName() + ">" + dir.fileName() + 
            "</a></td><td align='right'>" + dir.fileSize() +  "</td></tr>" ;
      */
    }
  }

  lstFile.sort(compareFiles);

  for (std::list<file>::iterator it=lstFile.begin() ; it != lstFile.end(); ++it)
  {
    str = str + "<tr><td align='left'><a href=" + (*it).name + ">" + (*it).name + 
          "</a></td><td align='right'>" + (*it).size + "</td></tr>" ;
  }

  lstFile.clear() ;

  str = str + "</table>"
              "</DIV>"
              "</body>"
              "</html>" ;
  server.send(200, "text/html", str);
}

// webserver functions
void handleNotFound()
{
#ifdef VERBOSE
  Serial.println(String("handleNotFound(") + server.uri() + ")");
#endif

#ifdef LITTLEFS
  if (LittleFS.exists(server.uri().substring(1)))
#else
  if (SPIFFS.exists(server.uri()))
#endif
  {
    // not defined -> use static file
    handleStaticFile(server.uri());
  }
  else
  {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    // server.send(404, "text/plain", message);
    // LH -> no errors -> goto start page instead
    handleRedirect("/index"); 
  }
}

void handleRedirect(String filename)
{
  char buf[64];
  filename.toCharArray(buf, sizeof(buf));
  handleRedirect(filename);
}

void handleRedirect(const char * filename)
{
#ifdef VERBOSE
  Serial.print("handleRedirect(");
  Serial.print(filename);
  Serial.println(")");
#endif
  server.sendHeader("Location", String(filename), true);
  server.send(302, "text/plain", "");
}

bool handleStaticFile(String path)
{
#ifdef VERBOSE
  Serial.println(String("handleStaticFile(") + path + ")");
#endif

#ifdef LITTLEFS
  path = path.substring(1);
#endif

  String contentType = getContentType(path);            // Get the MIME type

#ifdef LITTLEFS
  if (LittleFS.exists(path)) {                            // If the file exists
    File file = LittleFS.open(path, "r");                 // Open it
#else
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
#endif
    //size_t sent = 
    server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
#ifdef VERBOSE
  Serial.println("\tFile <" + path + "> not Found");
#endif
  return false;                                         // If the file doesn't exist, return false
}

bool handleStaticFile(const char * path) {
  return handleStaticFile((String)path);
}

void handleJSON() {
#ifdef VERBOSE
  Serial.println("handleJSON");
#endif
  String message = "HTTP Request\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
#ifdef VERBOSE
  Serial.println(message);
#endif

  // this gets called in response to either a PUT or a POST
  if (server.args() == 1 && server.hasArg("plain"))
  {
    // parse it as JSON object
    DynamicJsonDocument root(300);
    DeserializationError error = deserializeJson(root, server.arg("plain"));
    if (error) {
      handleStaticFile("/failure.html");
      return;
    }
    JSON_TO_CONFIG(universe, "universe");
    JSON_TO_CONFIG(channels, "channels");
    JSON_TO_CONFIG(delay, "delay");
    JSON_TO_CONFIG(useMaps, "useMaps");
    JSON_TO_CONFIG(mapChan, "mapChan");
    handleStaticFile("/success.html");
  }
  else {
    // parse it as key1=val1&key2=val2&key3=val3
    KEYVAL_TO_CONFIG(universe, "universe");
    KEYVAL_TO_CONFIG(channels, "channels");
    KEYVAL_TO_CONFIG(delay, "delay");
    KEYVAL_TO_CONFIG(useMaps, "useMaps");
    KEYVAL_TO_CONFIG(mapChan, "mapChan");
    handleStaticFile("/success.html");
  }
  saveConfig();
}
