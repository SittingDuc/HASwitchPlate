////////////////////////////////////////////////////////////////////////////////////////////////////
//           _____ _____ _____ _____
//          |  |  |  _  |   __|  _  |
//          |     |     |__   |   __|
//          |__|__|__|__|_____|__|
//        Home Automation Switch Plate
// https://github.com/aderusha/HASwitchPlate
//
// Copyright (c) 2019 Allen Derusha allen@derusha.org
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this hardware,
// software, and associated documentation files (the "Product"), to deal in the Product without
// restriction, including without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Product, and to permit persons to whom the
// Product is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Product.
//
// THE PRODUCT IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE PRODUCT OR THE USE OR OTHER DEALINGS IN THE PRODUCT.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "settings.h"
#include <Arduino.h>
#include <FS.h> // spifs is deprecated after 2.6.3, littleFS is new
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include "debug.h"
debugClass debug;         // our debug Object, USB Serial and/or Telnet

#include "config_class.h"
ConfigClass config; // our Configuration Container

#include "hmi_nextion.h"
hmiNextionClass nextion;  // our LCD Object

#include "mqtt_class.h"
MQTTClass mqtt;           // our MQTT Object

#include "web_class.h"
WebClass web; // our HTTP Server Object

#include "speaker_class.h"
SpeakerClass beep; // our Speaker Object


// Settings (initial values) now live in a separate file
// So we do not leave secrets here and upload to github accidentally


const char wifiConfigPass[9] = WIFI_CONFIG_PASSWORD; // First-time config WPA2 password
const char wifiConfigAP[14] = WIFI_CONFIG_AP;        // First-time config SSID
bool shouldSaveConfig = false;                       // Flag to save json config to SPIFFS
const uint32_t updateCheckInterval = UPDATE_CHECK_INTERVAL; // Time in msec between update checks (12 hours)
bool updateEspAvailable = false;                    // Flag for update check to report new ESP FW version
float updateEspAvailableVersion;                    // Float to hold the new ESP FW version number
bool updateLcdAvailable = false;                    // Flag for update check to report new LCD FW version
bool debugSerialD8Enabled = true;                   // Enable hardware serial debug output on pin D8
const uint32_t telnetInputMax = 128;                // Size of user input buffer for user telnet session
bool motionEnabled = false;                         // Motion sensor is enabled
bool mdnsEnabled = false;                           // mDNS enabled
uint8_t motionPin = 0;                              // GPIO input pin for motion sensor if connected and enabled
bool motionActive = false;                          // Motion is being detected
const uint32_t motionLatchTimeout = MOTION_LATCH_TIMEOUT;          // Latch time for motion sensor
const uint32_t motionBufferTimeout = MOTION_BUFFER_TIMEOUT;        // Latch time for motion sensor
uint32_t updateLcdAvailableVersion;                                // Int to hold the new LCD FW version number
const uint32_t connectTimeout = CONNECTION_TIMEOUT;       // Timeout for WiFi and MQTT connection attempts in seconds
const uint32_t reConnectTimeout = RECONNECT_TIMEOUT;      // Timeout for WiFi reconnection attempts in seconds
uint8_t espMac[6];                                        // Byte array to store our MAC address
uint32_t updateCheckTimer = 0;                      // Timer for update check

WiFiClient wifiClient;                     // client for OTA?
WiFiServer telnetServer(23);               // Server listening for Telnet
WiFiClient telnetClient;
MDNSResponder::hMDNSService hMDNSService;  // Bonjour

// URL for auto-update "version.json"
const char UPDATE_URL[] = DEFAULT_URL_UPDATE;
// Default link to compiled Arduino firmware image
String espFirmwareUrl = DEFAULT_URL_ARDUINO_FW;
// Default link to compiled Nextion firmware images
String lcdFirmwareUrl = DEFAULT_URL_LCD_FW;



////////////////////////////////////////////////////////////////////////////////////////////////////

String getSubtringField(String data, char separator, int index);
String printHex8(String data, uint8_t length);
void espReset();
void configRead();
void espWifiSetup();
void espWifiReconnect();
void espWifiConfigCallback(WiFiManager *myWiFiManager);
void espSetupOta();
void espStartOta(String espOtaUrl);
void configSaveCallback();
void configSave();
void configClearSaved();

void motionSetup();
void motionUpdate();

bool updateCheck();
void handleTelnetClient();


////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{ // System setup
  nextion.initResetPin();
  Serial.begin(115200);  // Serial - LCD RX (after swap), debug TX
  Serial1.begin(115200); // Serial1 - LCD TX, no RX
  Serial.swap();

  debug.begin();
  nextion.begin();

  debug.printLn(SYSTEM,String(F("SYSTEM: Starting HASwitchPlate v")) + String(config.getHaspVersion()));
  debug.printLn(SYSTEM,String(F("SYSTEM: Last reset reason: ")) + String(ESP.getResetInfo()));
  debug.printLn(SYSTEM,String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );
  debug.printLn(SYSTEM,String(F("SYSTEM: espCore: ")) + String(ESP.getCoreVersion()) );

  //configRead(); // Check filesystem for a saved config.json


  espWifiSetup(); // Start up networking

  debug.printLn(SYSTEM,String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );

  if (mdnsEnabled)
  { // Setup mDNS service discovery if enabled
    hMDNSService = MDNS.addService(config.getHaspNode(), "http", "tcp", 80);
    if (debug.getTelnetEnabled())
    {
      MDNS.addService(config.getHaspNode(), "telnet", "tcp", 23);
    }
    MDNS.addServiceTxt(hMDNSService, "app_name", "HASwitchPlate");
    MDNS.addServiceTxt(hMDNSService, "app_version", String(config.getHaspVersion()).c_str());
    MDNS.update();
  }

  web.begin();

  espSetupOta(); // Start OTA firmware update

  mqtt.begin();

  motionSetup(); // Setup motion sensor if configured

  beep.begin();

  if (debug.getTelnetEnabled())
  { // Setup telnet server for remote debug output
    telnetServer.setNoDelay(true);
    telnetServer.begin();
    debug.printLn(String(F("TELNET: debug server enabled at telnet:")) + WiFi.localIP().toString());
  }

  debug.printLn(SYSTEM,F("SYSTEM: System init complete."));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{ // Main execution loop

  nextion.loop();

  while ((WiFi.status() != WL_CONNECTED) || (WiFi.localIP().toString() == "0.0.0.0"))
  { // Check WiFi is connected and that we have a valid IP, retry until we do.
    if (WiFi.status() == WL_CONNECTED)
    { // If we're currently connected, disconnect so we can try again
      WiFi.disconnect();
    }
    espWifiReconnect();
  }

  mqtt.loop();

  ArduinoOTA.handle();      // Arduino OTA loop
  web.loop();
  if (mdnsEnabled)
  {
    MDNS.update();
  }

  if ((millis() - updateCheckTimer) >= updateCheckInterval)
  { // Run periodic update check
    updateCheckTimer = millis();
    if (updateCheck())
    { // Send a status update if the update check worked
      mqtt.statusUpdate();
    }
  }

  if (motionEnabled)
  { // Check on our motion sensor
    motionUpdate();
  }

  if (debug.getTelnetEnabled())
  {
    handleTelnetClient(); // telnetClient loop
  }

  beep.loop();
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions


////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiSetup()
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

    wifiManager.setAPCallback(espWifiConfigCallback);

    // Fetches SSID and pass from EEPROM and tries to connect
    // If it does not connect it starts an access point with the specified name
    // and goes into a blocking loop awaiting configuration.
    if (!wifiManager.autoConnect(wifiConfigAP, wifiConfigPass))
    { // Reset and try again
      debug.printLn(WIFI,F("WIFI: Failed to connect and hit timeout"));
      espReset();
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

    if (shouldSaveConfig)
    { // Save the custom parameters to FS
      configSave();
    }
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
        espReset();
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
void espWifiReconnect()
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
      espReset();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiConfigCallback(WiFiManager *myWiFiManager)
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
void espSetupOta()
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
      espReset();
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
void espStartOta(String espOtaUrl)
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
    espReset();
  }
  delay(5000);
  nextion.sendCmd("page " + String(nextion.getActivePage()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espReset()
{
  debug.printLn(F("RESET: HASP reset"));
  mqtt.goodbye();
  nextion.reset();
  ESP.reset();
  delay(5000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configRead()
{ // Read saved config.json from SPIFFS
  debug.printLn(F("SPIFFS: mounting SPIFFS"));
  if (SPIFFS.begin())
  {
    if (SPIFFS.exists("/config.json"))
    { // File exists, reading and loading
      debug.printLn(F("SPIFFS: reading /config.json"));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        size_t configFileSize = configFile.size(); // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize]);
        configFile.readBytes(buf.get(), configFileSize);

        DynamicJsonDocument configJson(1024);
        DeserializationError jsonError = deserializeJson(configJson, buf.get());
        if (jsonError)
        { // Couldn't parse the saved config
          debug.printLn(String(F("SPIFFS: [ERROR] Failed to parse /config.json: ")) + String(jsonError.c_str()));
        }
        else
        {
          if (!configJson["mqttServer"].isNull())
          {
            config.setMQTTServer(configJson["mqttServer"]);
          }
          if (!configJson["mqttPort"].isNull())
          {
            config.setMQTTPort(configJson["mqttPort"]);
          }
          if (!configJson["mqttUser"].isNull())
          {
            config.setMQTTUser(configJson["mqttUser"]);
          }
          if (!configJson["mqttPassword"].isNull())
          {
            config.setMQTTPassword(configJson["mqttPassword"]);
          }
          if (!configJson["haspNode"].isNull())
          {
            config.setHaspNode(configJson["haspNode"]);
          }
          if (!configJson["groupName"].isNull())
          {
            config.setGroupName(configJson["groupName"]);
          }
          if (!configJson["configUser"].isNull())
          {
            web.setUser(configJson["configUser"]);
          }
          if (!configJson["configPassword"].isNull())
          {
            web.setPassword(configJson["configPassword"]);
          }
          if (!configJson["motionPinConfig"].isNull())
          {
            config.setMotionPin(configJson["motionPinConfig"]);
          }
          if (!configJson["debugSerialEnabled"].isNull())
          {
            debug.enableSerial(configJson["debugSerialEnabled"]); // debug, config, or both?
          }
          if (!configJson["debugTelnetEnabled"].isNull())
          {
            debug.enableTelnet(configJson["debugTelnetEnabled"]); // debug, config, or both?
          }
          if (!configJson["mdnsEnabled"].isNull())
          {
            if (configJson["mdnsEnabled"]) // local or config?
            {
              mdnsEnabled = true;
            }
            else
            {
              mdnsEnabled = false;
            }
          }
          if (!configJson["beepEnabled"].isNull())
          {
            beep.enable(configJson["beepEnabled"]);
          }
          String configJsonStr;
          serializeJson(configJson, configJsonStr);
          debug.printLn(String(F("SPIFFS: parsed json:")) + configJsonStr);
        }
      }
      else
      {
        debug.printLn(F("SPIFFS: [ERROR] Failed to read /config.json"));
      }
    }
    else
    {
      debug.printLn(F("SPIFFS: [WARNING] /config.json not found, will be created on first config save"));
    }
  }
  else
  {
    debug.printLn(F("SPIFFS: [ERROR] Failed to mount FS"));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configSaveCallback()
{ // Callback notifying us of the need to save config
  debug.printLn(F("SPIFFS: Configuration changed, flagging for save"));
  shouldSaveConfig = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configSave()
{ // Save the custom parameters to config.json
  nextion.setAttr("p[0].b[1].txt", "\"Saving\\rconfig\"");
  debug.printLn(F("SPIFFS: Saving config"));
  DynamicJsonDocument jsonConfigValues(1024);
  jsonConfigValues["mqttServer"] = config.getMQTTServer();
  jsonConfigValues["mqttPort"] = config.getMQTTPort();
  jsonConfigValues["mqttUser"] = config.getMQTTUser();
  jsonConfigValues["mqttPassword"] = config.getMQTTPassword();
  jsonConfigValues["haspNode"] = config.getHaspNode();
  jsonConfigValues["groupName"] = config.getGroupName();
  jsonConfigValues["configUser"] = web.getUser();
  jsonConfigValues["configPassword"] = web.getPassword();
  jsonConfigValues["motionPinConfig"] = config.getMotionPin();
  jsonConfigValues["debugSerialEnabled"] = debug.getSerialEnabled();
  jsonConfigValues["debugTelnetEnabled"] = debug.getTelnetEnabled();
  jsonConfigValues["mdnsEnabled"] = mdnsEnabled;
  jsonConfigValues["beepEnabled"] = beep.getEnable();

  debug.printLn(String(F("SPIFFS: mqttServer = ")) + String(config.getMQTTServer()));
  debug.printLn(String(F("SPIFFS: mqttPort = ")) + String(config.getMQTTPort()));
  debug.printLn(String(F("SPIFFS: mqttUser = ")) + String(config.getMQTTUser()));
  debug.printLn(String(F("SPIFFS: mqttPassword = ")) + String(config.getMQTTPassword()));
  debug.printLn(String(F("SPIFFS: haspNode = ")) + String(config.getHaspNode()));
  debug.printLn(String(F("SPIFFS: groupName = ")) + String(config.getGroupName()));
  debug.printLn(String(F("SPIFFS: configUser = ")) + String(web.getUser()));
  debug.printLn(String(F("SPIFFS: configPassword = ")) + String(web.getPassword()));
  debug.printLn(String(F("SPIFFS: motionPinConfig = ")) + String(config.getMotionPin()));
  debug.printLn(String(F("SPIFFS: debugSerialEnabled = ")) + String(debug.getSerialEnabled()));
  debug.printLn(String(F("SPIFFS: debugTelnetEnabled = ")) + String(debug.getTelnetEnabled()));
  debug.printLn(String(F("SPIFFS: mdnsEnabled = ")) + String(mdnsEnabled));
  debug.printLn(String(F("SPIFFS: beepEnabled = ")) + String(beep.getEnable()));

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    debug.printLn(F("SPIFFS: Failed to open config file for writing"));
  }
  else
  {
    serializeJson(jsonConfigValues, configFile);
    configFile.close();
  }
  shouldSaveConfig = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configClearSaved()
{ // Clear out all local storage
  nextion.setAttr("dims", "100");
  nextion.sendCmd("page 0");
  nextion.setAttr("p[0].b[1].txt", "\"Resetting\\rsystem...\"");
  debug.printLn(F("RESET: Formatting SPIFFS"));
  SPIFFS.format();
  debug.printLn(F("RESET: Clearing WiFiManager settings..."));
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  EEPROM.begin(512);
  debug.printLn(F("Clearing EEPROM..."));
  for (uint16_t i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
  nextion.setAttr("p[0].b[1].txt", "\"Rebooting\\rsystem...\"");
  debug.printLn(F("RESET: Rebooting device"));
  espReset();
}



////////////////////////////////////////////////////////////////////////////////////////////////////
bool updateCheck()
{ // firmware update check
  // Gerard has placed nodes on an isolated network. Connecting to the live internet is verboten and unpossible. So skip the pointless check
#ifdef DISABLE_UPDATE_CHECK
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
void motionSetup()
{
  if (strcmp(config.getMotionPin(), "D0") == 0)
  {
    motionEnabled = true;
    motionPin = D0;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(config.getMotionPin(), "D1") == 0)
  {
    motionEnabled = true;
    motionPin = D1;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(config.getMotionPin(), "D2") == 0)
  {
    motionEnabled = true;
    motionPin = D2;
    pinMode(motionPin, INPUT);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void motionUpdate()
{
  static uint32_t motionLatchTimer = 0;         // Timer for motion sensor latch
  static uint32_t motionBufferTimer = millis(); // Timer for motion sensor buffer
  static bool motionActiveBuffer = motionActive;
  bool motionRead = digitalRead(motionPin);

  if (motionRead != motionActiveBuffer)
  { // if we've changed state
    motionBufferTimer = millis();
    motionActiveBuffer = motionRead;
  }
  else if (millis() > (motionBufferTimer + motionBufferTimeout))
  {
    if ((motionActiveBuffer && !motionActive) && (millis() > (motionLatchTimer + motionLatchTimeout)))
    {
      motionLatchTimer = millis();
      mqtt.publishMotionTopic(String(F("ON")));
      motionActive = motionActiveBuffer;
      debug.printLn("MOTION: Active");
    }
    else if ((!motionActiveBuffer && motionActive) && (millis() > (motionLatchTimer + motionLatchTimeout)))
    {
      motionLatchTimer = millis();
      mqtt.publishMotionTopic(String(F("OFF")));

      motionActive = motionActiveBuffer;
      debug.printLn("MOTION: Inactive");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void handleTelnetClient()
{ // Basic telnet client handling code from: https://gist.github.com/tablatronix/4793677ca748f5f584c95ec4a2b10303
  static uint32_t telnetInputIndex = 0;
  if (telnetServer.hasClient())
  { // client is connected
    if (!telnetClient || !telnetClient.connected())
    {
      if (telnetClient)
      {
        telnetClient.stop(); // client disconnected
      }
      telnetClient = telnetServer.available(); // ready for new client
      telnetInputIndex = 0;                    // reset input buffer index
    }
    else
    {
      telnetServer.available().stop(); // have client, block new connections
    }
  }
  // Handle client input from telnet connection.
  if (telnetClient && telnetClient.connected() && telnetClient.available())
  { // client input processing
    static char telnetInputBuffer[telnetInputMax];

    if (telnetClient.available())
    {
      char telnetInputByte = telnetClient.read(); // Read client byte
      // debug.printLn(String("telnet in: 0x") + String(telnetInputByte, HEX));
      if (telnetInputByte == 5)
      { // If the telnet client sent a bunch of control commands on connection (which end in ENQUIRY/0x05), ignore them and restart the buffer
        telnetInputIndex = 0;
      }
      else if (telnetInputByte == 13)
      { // telnet line endings should be CRLF: https://tools.ietf.org/html/rfc5198#appendix-C
        // If we get a CR just ignore it
      }
      else if (telnetInputByte == 10)
      {                                          // We've caught a LF (DEC 10), send buffer contents to the Nextion
        telnetInputBuffer[telnetInputIndex] = 0; // null terminate our char array
        nextion.sendCmd(String(telnetInputBuffer));
        telnetInputIndex = 0;
      }
      else if (telnetInputIndex < telnetInputMax)
      { // If we have room left in our buffer add the current byte
        telnetInputBuffer[telnetInputIndex] = telnetInputByte;
        telnetInputIndex++;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Submitted by benmprojects to handle "beep" commands. Split
// incoming String by separator, return selected field as String
// Original source: https://arduino.stackexchange.com/a/1237
String getSubtringField(String data, char separator, int index)
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
String printHex8(uint8_t *data, uint8_t length)
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
