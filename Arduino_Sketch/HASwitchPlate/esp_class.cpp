// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// config_class.cpp : Class internals to support low-level ESP8266 functions
//
// ----------------------------------------------------------------------------------------------------------------- //

#include "common.h"
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>


// TODO: Class These!
extern const char wifiConfigPass[9] = WIFI_CONFIG_PASSWORD; // First-time config WPA2 password
extern const char wifiConfigAP[14] = WIFI_CONFIG_AP;        // First-time config SSID
extern const uint32_t connectTimeout = CONNECTION_TIMEOUT;       // Timeout for WiFi and MQTT connection attempts in seconds
extern const uint32_t reConnectTimeout = RECONNECT_TIMEOUT;      // Timeout for WiFi reconnection attempts in seconds
uint8_t espMac[6];                                        // Byte array to store our MAC address

bool updateEspAvailable = false;                    // Flag for update check to report new ESP FW version
float updateEspAvailableVersion;                    // Float to hold the new ESP FW version number
bool updateLcdAvailable = false;                    // Flag for update check to report new LCD FW version
uint32_t updateLcdAvailableVersion;                 // Int to hold the new LCD FW version number

// URL for auto-update "version.json"
const char UPDATE_URL[] = DEFAULT_URL_UPDATE;
// Default link to compiled Arduino firmware image
String espFirmwareUrl = DEFAULT_URL_ARDUINO_FW;
// Default link to compiled Nextion firmware images
String lcdFirmwareUrl = DEFAULT_URL_LCD_FW;



// is this one local or remote?
extern WiFiClient wifiClient;                     // client for OTA?

// a reference to our global copy of self, so we can make working callbacks
extern ourEspClass esp;

////////////////////////////////////////////////////////////////////////////////////////////////////
static void configSaveCallback()
{ // Callback notifying our config class of the need to save config
  config.saveCallback();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void configWiFiCallback(WiFiManager *myWiFiManager)
{ // Notify the user that we're entering config mode
  debug.printLn(WIFI,F("WIFI: Failed to connect to assigned AP, entering config mode"));
  while (millis() < 800)
  { // for factory-reset system this will be called before display is responsive. give it a second.
    delay(10);
  }
  nextion.sendCmd("page 0");
  nextion.setAttr("p[0].b[1].font", "6");
  nextion.setAttr("p[0].b[1].txt", "\" HASP WiFi Setup\\r AP: " + String(wifiConfigAP) + "\\rPassword: " + String(wifiConfigPass) + "\\r\\r\\r\\r\\r\\r\\r  http://192.168.4.1\"");
  nextion.sendCmd("vis 3,1");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void resetCallback()
{ // callback to reset the micro
    esp.reset();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void ourEspClass::begin()
{ // called in the main code setup, handles our initialisation
  wiFiSetup(); // Start up networking
  // in the original setup() routine, there were other calls here
  // so we have bought setupOTA forward in time...
  setupOta(); // Start OTA firmware update
}

void ourEspClass::loop()
{ // called in the main code loop, handles our periodic code
  while ((WiFi.status() != WL_CONNECTED) || (WiFi.localIP().toString() == "0.0.0.0"))
  { // Check WiFi is connected and that we have a valid IP, retry until we do.
    if (WiFi.status() == WL_CONNECTED)
    { // If we're currently connected, disconnect so we can try again
      WiFi.disconnect();
    }
    wiFiReconnect();
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ourEspClass::reset()
{
  debug.printLn(F("RESET: HASP reset"));
  mqtt.goodbye();
  nextion.reset();
  ESP.reset();
  delay(5000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ourEspClass::wiFiSetup()
{ // Connect to WiFi
  nextion.sendCmd("page 0");
  nextion.setAttr("p[0].b[1].font", "6");
  nextion.setAttr("p[0].b[1].txt", "\"WiFi Connecting...\\r " + String(WiFi.SSID()) + "\"");

  WiFi.macAddress(espMac);            // Read our MAC address and save it to espMac
  WiFi.hostname(config.getHaspNode());            // Assign our hostname before connecting to WiFi
  WiFi.setAutoReconnect(true);        // Tell WiFi to autoreconnect if connection has dropped
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable WiFi sleep modes to prevent occasional disconnects

  if (String(config.getWIFISSID()) == "")
  { // If the sketch has not defined a static wifiSSID use WiFiManager to collect required information from the user.

    // id/name, placeholder/prompt, default value, length, extra tags
    WiFiManagerParameter custom_haspNodeHeader("<br/><br/><b>HASP Node Name</b>");
    WiFiManagerParameter custom_haspNode("haspNode", "HASP Node (required. lowercase letters, numbers, and _ only)", config.getHaspNode(), 15, " maxlength=15 required pattern='[a-z0-9_]*'");
    WiFiManagerParameter custom_groupName("groupName", "Group Name (required)", config.getGroupName(), 15, " maxlength=15 required");
    WiFiManagerParameter custom_mqttHeader("<br/><br/><b>MQTT Broker</b>");
    WiFiManagerParameter custom_mqttServer("mqttServer", "MQTT Server", config.getMQTTServer(), 63, " maxlength=39");
    WiFiManagerParameter custom_mqttPort("mqttPort", "MQTT Port", config.getMQTTPort(), 5, " maxlength=5 type='number'");
    WiFiManagerParameter custom_mqttUser("mqttUser", "MQTT User", config.getMQTTUser(), 31, " maxlength=31");
    WiFiManagerParameter custom_mqttPassword("mqttPassword", "MQTT Password", config.getMQTTPassword(), 31, " maxlength=31 type='password'");
    WiFiManagerParameter custom_configHeader("<br/><br/><b>Admin access</b>");
    WiFiManagerParameter custom_configUser("configUser", "Config User", web.getUser(), 15, " maxlength=31'");
    WiFiManagerParameter custom_configPassword("configPassword", "Config Password", web.getPassword(), 31, " maxlength=31 type='password'");

    WiFiManager wifiManager;
    wifiManager.setSaveConfigCallback(configSaveCallback); // set config save notify callback
    wifiManager.setCustomHeadElement(web.getHaspStyle());          // add custom style
    wifiManager.addParameter(&custom_haspNodeHeader);
    wifiManager.addParameter(&custom_haspNode);
    wifiManager.addParameter(&custom_groupName);
    wifiManager.addParameter(&custom_mqttHeader);
    wifiManager.addParameter(&custom_mqttServer);
    wifiManager.addParameter(&custom_mqttPort);
    wifiManager.addParameter(&custom_mqttUser);
    wifiManager.addParameter(&custom_mqttPassword);
    wifiManager.addParameter(&custom_configHeader);
    wifiManager.addParameter(&custom_configUser);
    wifiManager.addParameter(&custom_configPassword);

    // Timeout config portal after connectTimeout seconds, useful if configured wifi network was temporarily unavailable
    wifiManager.setTimeout(connectTimeout);

    wifiManager.setAPCallback(configWiFiCallback);

    // Fetches SSID and pass from EEPROM and tries to connect
    // If it does not connect it starts an access point with the specified name
    // and goes into a blocking loop awaiting configuration.
    if (!wifiManager.autoConnect(wifiConfigAP, wifiConfigPass))
    { // Reset and try again
      debug.printLn(WIFI,F("WIFI: Failed to connect and hit timeout"));
      reset();
    }

    // Read updated parameters
    config.setMQTTServer(custom_mqttServer.getValue());
    config.setMQTTPort(custom_mqttPort.getValue());
    config.setMQTTUser(custom_mqttUser.getValue());
    config.setMQTTPassword(custom_mqttPassword.getValue());
    config.setHaspNode(custom_haspNode.getValue());
    config.setGroupName(custom_groupName.getValue());
    web.setUser(custom_configUser.getValue());
    web.setPassword(custom_configPassword.getValue());

    config.saveFileIfNeeded();
  }
  else
  { // wifiSSID has been defined, so attempt to connect to it forever
    debug.printLn(WIFI,String(F("Connecting to WiFi network: ")) + String(config.getWIFISSID()));
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.getWIFISSID(), config.getWIFIPass());

    uint32_t wifiReconnectTimer = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      if (millis() >= (wifiReconnectTimer + (connectTimeout * ASECOND)))
      { // If we've been trying to reconnect for connectTimeout seconds, reboot and try again
        debug.printLn(WIFI,F("WIFI: Failed to connect and hit timeout"));
        reset();
      }
    }
  }
  // If you get here you have connected to WiFi
  nextion.setAttr("p[0].b[1].font", "6");
  nextion.setAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\"");
  debug.printLn(WIFI,String(F("WIFI: Connected successfully and assigned IP: ")) + WiFi.localIP().toString());
  if (nextion.getActivePage())
  {
    nextion.sendCmd("page " + String(nextion.getActivePage()));
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void ourEspClass::wiFiReconnect()
{ // Existing WiFi connection dropped, try to reconnect
  debug.printLn(WIFI,F("Reconnecting to WiFi network..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.getWIFISSID(), config.getWIFIPass());

  uint32_t wifiReconnectTimer = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (millis() >= (wifiReconnectTimer + (reConnectTimeout * ASECOND)))
    { // If we've been trying to reconnect for reConnectTimeout seconds, reboot and try again
      debug.printLn(WIFI,F("WIFI: Failed to reconnect and hit timeout"));
      reset();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ourEspClass::setupOta()
{ // (mostly) boilerplate OTA setup from library examples

  ArduinoOTA.setHostname(config.getHaspNode());
  ArduinoOTA.setPassword(web.getPassword());

  ArduinoOTA.onStart([]() {
      debug.printLn(F("ESP OTA: update start"));
      nextion.sendCmd("page 0");
      nextion.setAttr("p[0].b[1].txt", "\"ESP OTA Update\"");
    });
  ArduinoOTA.onEnd([]() {
      nextion.sendCmd("page 0");
      debug.printLn(F("ESP OTA: update complete"));
      nextion.setAttr("p[0].b[1].txt", "\"ESP OTA Update\\rComplete!\"");
      resetCallback();
    });
  ArduinoOTA.onProgress([](uint32_t progress, uint32_t total) {
      nextion.setAttr("p[0].b[1].txt", "\"ESP OTA Update\\rProgress: " + String(progress / (total / 100)) + "%\"");
    });
  ArduinoOTA.onError([](ota_error_t error) {
      debug.printLn(String(F("ESP OTA: ERROR code ")) + String(error));
      if (error == OTA_AUTH_ERROR)
        debug.printLn(F("ESP OTA: ERROR - Auth Failed"));
      else if (error == OTA_BEGIN_ERROR)
        debug.printLn(F("ESP OTA: ERROR - Begin Failed"));
      else if (error == OTA_CONNECT_ERROR)
        debug.printLn(F("ESP OTA: ERROR - Connect Failed"));
      else if (error == OTA_RECEIVE_ERROR)
        debug.printLn(F("ESP OTA: ERROR - Receive Failed"));
      else if (error == OTA_END_ERROR)
        debug.printLn(F("ESP OTA: ERROR - End Failed"));
      nextion.setAttr("p[0].b[1].txt", "\"ESP OTA FAILED\"");
      delay(5000);
      nextion.sendCmd("page " + String(nextion.getActivePage()));
    });
  ArduinoOTA.begin();
  debug.printLn(F("ESP OTA: Over the Air firmware update ready"));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void ourEspClass::startOta(String espOtaUrl)
{ // Update ESP firmware from HTTP
  nextion.sendCmd("page 0");
  nextion.setAttr("p[0].b[1].txt", "\"HTTP update\\rstarting...\"");
  WiFiUDP::stopAll(); // Keep mDNS responder from breaking things

  t_httpUpdate_return returnCode = ESPhttpUpdate.update(wifiClient, espOtaUrl);
  switch (returnCode)
  {
  case HTTP_UPDATE_FAILED:
    debug.printLn("ESPFW: HTTP_UPDATE_FAILED error " + String(ESPhttpUpdate.getLastError()) + " " + ESPhttpUpdate.getLastErrorString());
    nextion.setAttr("p[0].b[1].txt", "\"HTTP Update\\rFAILED\"");
    break;

  case HTTP_UPDATE_NO_UPDATES:
    debug.printLn(F("ESPFW: HTTP_UPDATE_NO_UPDATES"));
    nextion.setAttr("p[0].b[1].txt", "\"HTTP Update\\rNo update\"");
    break;

  case HTTP_UPDATE_OK:
    debug.printLn(F("ESPFW: HTTP_UPDATE_OK"));
    nextion.setAttr("p[0].b[1].txt", "\"HTTP Update\\rcomplete!\\r\\rRestarting.\"");
    reset();
  }
  delay(5000);
  nextion.sendCmd("page " + String(nextion.getActivePage()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ourEspClass::updateCheck()
{ // firmware update check
  // Gerard has placed nodes on an isolated network. Connecting to the live internet is verboten and unpossible. So skip the pointless check
#if UPDATE_CHECK_ENABLE==(false)
  updateEspAvailable = false;
  updateLcdAvailable = false;
#else
  HTTPClient updateClient;
  debug.printLn(String(F("UPDATE: Checking update URL: ")) + String(UPDATE_URL));
  String updatePayload;
  updateClient.begin(wifiClient, UPDATE_URL);
  int httpCode = updateClient.GET(); // start connection and send HTTP header

  if (httpCode > 0)
  { // httpCode will be negative on error
  if (httpCode == HTTP_CODE_OK)
  { // file found at server
  updatePayload = updateClient.getString();
  }
  }
  else
  {
  debug.printLn(String(F("UPDATE: Update check failed: ")) + updateClient.errorToString(httpCode));
  return false;
  }
  updateClient.end();

  DynamicJsonDocument updateJson(2048);
  DeserializationError jsonError = deserializeJson(updateJson, updatePayload);

  if (jsonError)
  { // Couldn't parse the returned JSON, so bail
  debug.printLn(String(F("UPDATE: JSON parsing failed: ")) + String(jsonError.c_str()));
  return false;
  }
  else
  {
  if (!updateJson["d1_mini"]["version"].isNull())
  {
  updateEspAvailableVersion = updateJson["d1_mini"]["version"].as<float>();
  debug.printLn(String(F("UPDATE: updateEspAvailableVersion: ")) + String(updateEspAvailableVersion));
  espFirmwareUrl = updateJson["d1_mini"]["firmware"].as<String>();
  if (updateEspAvailableVersion > config.getHaspVersion())
  {
  updateEspAvailable = true;
  debug.printLn(String(F("UPDATE: New ESP version available: ")) + String(updateEspAvailableVersion));
  }
  }
  if (nextion.getModel() && !updateJson[nextion.getModel()]["version"].isNull())
  {
  updateLcdAvailableVersion = updateJson[nextion.getModel()]["version"].as<int>();
  debug.printLn(String(F("UPDATE: updateLcdAvailableVersion: ")) + String(updateLcdAvailableVersion));
  lcdFirmwareUrl = updateJson[nextion.getModel()]["firmware"].as<String>();
  if (updateLcdAvailableVersion > nextion.getLCDVersion())
  {
  updateLcdAvailable = true;
  debug.printLn(String(F("UPDATE: New LCD version available: ")) + String(updateLcdAvailableVersion));
  }
  }
  debug.printLn(F("UPDATE: Update check completed"));
  }
#endif
  return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Submitted by benmprojects to handle "beep" commands. Split
// incoming String by separator, return selected field as String
// Original source: https://arduino.stackexchange.com/a/1237
String ourEspClass::getSubtringField(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length();

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

////////////////////////////////////////////////////////////////////////////////
String ourEspClass::printHex8(uint8_t *data, uint8_t length)
{ // returns input bytes as printable hex values in the format 01 23 FF

  String hex8String;
  for (int i = 0; i < length; i++)
  {
    // hex8String += "0x";
    if (data[i] < 0x10)
    {
      hex8String += "0";
    }
    hex8String += String(data[i], HEX);
    if (i != (length - 1))
    {
      hex8String += " ";
    }
  }
  hex8String.toUpperCase();
  return hex8String;
}
