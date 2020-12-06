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
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <MQTT.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include "debug.h"
#include "hmi_nextion.h"

// Settings now in a separate file
// So we do not leave secrets here and upload to github accidentally

const float haspVersion = HASP_VERSION;              // Current HASP software release version
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
bool beepEnabled = false;                           // Keypress beep enabled
uint32_t beepOnTime = BEEP_DEFAULT_TIME;            // milliseconds of on-time for beep
uint32_t beepOffTime = BEEP_DEFAULT_TIME;           // milliseconds of off-time for beep
bool beepState;                                     // beep currently engaged
uint32_t beepCounter;                               // Count the number of beeps
uint8_t beepPin;                                    // define beep pin output
uint8_t motionPin = 0;                              // GPIO input pin for motion sensor if connected and enabled
bool motionActive = false;                          // Motion is being detected
const uint32_t motionLatchTimeout = MOTION_LATCH_TIMEOUT;          // Latch time for motion sensor
const uint32_t motionBufferTimeout = MOTION_BUFFER_TIMEOUT;        // Latch time for motion sensor
uint32_t updateLcdAvailableVersion;                                // Int to hold the new LCD FW version number
const int32_t statusUpdateInterval = MQTT_STATUS_UPDATE_INTERVAL;  // Time in msec between publishing MQTT status updates (5 minutes)
const uint32_t connectTimeout = CONNECTION_TIMEOUT;       // Timeout for WiFi and MQTT connection attempts in seconds
const uint32_t reConnectTimeout = RECONNECT_TIMEOUT;      // Timeout for WiFi reconnection attempts in seconds
uint8_t espMac[6];                                        // Byte array to store our MAC address
const uint16_t mqttMaxPacketSize = MQTT_MAX_PACKET_SIZE;  // Size of buffer for incoming MQTT message
uint32_t statusUpdateTimer = 0;                           // Timer for update check
uint32_t updateCheckTimer = 0;                      // Timer for update check
uint32_t beepTimer = 0;                             // will store last time beep was updated
String mqttClientId;                                // Auto-generated MQTT ClientID
String mqttGetSubtopic;                             // MQTT subtopic for incoming commands requesting .val
String mqttGetSubtopicJSON;                         // MQTT object buffer for JSON status when requesting .val
String mqttStateTopic;                              // MQTT topic for outgoing panel interactions
String mqttStateJSONTopic;                          // MQTT topic for outgoing panel interactions in JSON format
String mqttCommandTopic;                            // MQTT topic for incoming panel commands
String mqttGroupCommandTopic;                       // MQTT topic for incoming group panel commands
String mqttStatusTopic;                             // MQTT topic for publishing device connectivity state
String mqttSensorTopic;                             // MQTT topic for publishing device information in JSON format
String mqttLightCommandTopic;                       // MQTT topic for incoming panel backlight on/off commands
String mqttBeepCommandTopic;                        // MQTT topic for error beep
String mqttLightStateTopic;                         // MQTT topic for outgoing panel backlight on/off state
String mqttLightBrightCommandTopic;                 // MQTT topic for incoming panel backlight dimmer commands
String mqttLightBrightStateTopic;                   // MQTT topic for outgoing panel backlight dimmer state
String mqttMotionStateTopic;                        // MQTT topic for outgoing motion sensor state
String nextionModel;                                // Record reported model number of LCD panel
uint32_t tftFileSize = 0;                           // Filesize for TFT firmware upload
uint8_t nextionResetPin = D6;                       // Pin for Nextion power rail switch (GPIO12/D6)

WiFiClient wifiClient;                     // client for OTA?
WiFiClient wifiMQTTClient;                 // client for MQTT
MQTTClient mqttClient(mqttMaxPacketSize);  // MQTT Object
ESP8266WebServer webServer(80);            // Server listening for HTTP
ESP8266HTTPUpdateServer httpOTAUpdate;
WiFiServer telnetServer(23);               // Server listening for Telnet
WiFiClient telnetClient;
MDNSResponder::hMDNSService hMDNSService;  // Bonjour

hmiNextionClass nextion;  // our LCD Object
debugClass debug;         // our debug Object, USB Serial and/or Telnet


// Additional CSS style to match Hass theme
const char HASP_STYLE[] = "<style>button{background-color:#03A9F4;}body{width:60%;margin:auto;}input:invalid{border:1px solid red;}input[type=checkbox]{width:20px;}</style>";
// URL for auto-update "version.json"
const char UPDATE_URL[] = "http://haswitchplate.com/update/version.json";
// Default link to compiled Arduino firmware image
String espFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.ino.d1_mini.bin";
// Default link to compiled Nextion firmware images
String lcdFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.tft";


////////////////////////////////////////////////////////////////////////////////////////////////////


void mqttStatusUpdate();
void mqttCallback(String &strTopic, String &strPayload);
void mqttConnect();

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

void webHandleNotFound();
void webHandleRoot();
void webHandleSaveConfig();
void webHandleResetConfig();
void webHandleResetBacklight();
void webHandleFirmware();
void webHandleEspFirmware();
void webHandleLcdUpload();
void webHandleLcdUpdateSuccess();
void webHandleLcdUpdateFailure();
void webHandleLcdDownload();
void webHandleTftFileSize();
void webHandleReboot();
bool updateCheck();
void handleTelnetClient();


////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{ // System setup
  pinMode(nextionResetPin, OUTPUT);
  digitalWrite(nextionResetPin, HIGH);
  Serial.begin(115200);  // Serial - LCD RX (after swap), debug TX
  Serial1.begin(115200); // Serial1 - LCD TX, no RX
  Serial.swap();

  debug.begin();
  nextion.begin();

  debug.printLn(String(F("SYSTEM: Starting HASwitchPlate v")) + String(haspVersion));
  debug.printLn(String(F("SYSTEM: Last reset reason: ")) + String(ESP.getResetInfo()));
  debug.printLn(String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );
  debug.printLn(String(F("SYSTEM: espCore: ")) + String(ESP.getCoreVersion()) );

  //configRead(); // Check filesystem for a saved config.json


  espWifiSetup(); // Start up networking

  debug.printLn(String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );

  if (mdnsEnabled)
  { // Setup mDNS service discovery if enabled
    hMDNSService = MDNS.addService(haspNode, "http", "tcp", 80);
    if (debug.getTelnetEnabled())
    {
      MDNS.addService(haspNode, "telnet", "tcp", 23);
    }
    MDNS.addServiceTxt(hMDNSService, "app_name", "HASwitchPlate");
    MDNS.addServiceTxt(hMDNSService, "app_version", String(haspVersion).c_str());
    MDNS.update();
  }

  if ((configPassword[0] != '\0') && (configUser[0] != '\0'))
  { // Start the webserver with our assigned password if it's been configured...
    httpOTAUpdate.setup(&webServer, "/update", configUser, configPassword);
  }
  else
  { // or without a password if not
    httpOTAUpdate.setup(&webServer, "/update");
  }
  webServer.on("/", webHandleRoot);
  webServer.on("/saveConfig", webHandleSaveConfig);
  webServer.on("/resetConfig", webHandleResetConfig);
  webServer.on("/resetBacklight", webHandleResetBacklight);
  webServer.on("/firmware", webHandleFirmware);
  webServer.on("/espfirmware", webHandleEspFirmware);
  webServer.on("/lcdupload", HTTP_POST, []() { webServer.send(200); }, webHandleLcdUpload);
  webServer.on("/tftFileSize", webHandleTftFileSize);
  webServer.on("/lcddownload", webHandleLcdDownload);
  webServer.on("/lcdOtaSuccess", webHandleLcdUpdateSuccess);
  webServer.on("/lcdOtaFailure", webHandleLcdUpdateFailure);
  webServer.on("/reboot", webHandleReboot);
  webServer.onNotFound(webHandleNotFound);
  webServer.begin();
  debug.printLn(String(F("HTTP: Server started @ http://")) + WiFi.localIP().toString());

  espSetupOta(); // Start OTA firmware update

  mqttClient.begin(mqttServer, atoi(mqttPort), wifiMQTTClient); // Create MQTT service object
  mqttClient.onMessage(mqttCallback);                           // Setup MQTT callback function
  mqttConnect();                                                // Connect to MQTT

  motionSetup(); // Setup motion sensor if configured

  if (beepEnabled)
  { // Setup beep/tactile if configured
    beepPin = 4;
    pinMode(beepPin, OUTPUT);
  }

  if (debug.getTelnetEnabled())
  { // Setup telnet server for remote debug output
    telnetServer.setNoDelay(true);
    telnetServer.begin();
    debug.printLn(String(F("TELNET: debug server enabled at telnet:")) + WiFi.localIP().toString());
  }

  debug.printLn(F("SYSTEM: System init complete."));
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

  if (!mqttClient.connected())
  { // Check MQTT connection
    debug.printLn("MQTT: not connected, connecting.");
    mqttConnect();
  }

  mqttClient.loop();        // MQTT client loop
  ArduinoOTA.handle();      // Arduino OTA loop
  webServer.handleClient(); // webServer loop
  if (mdnsEnabled)
  {
    MDNS.update();
  }

  if ((millis() - statusUpdateTimer) >= statusUpdateInterval)
  { // Run periodic status update
    statusUpdateTimer = millis();
    mqttStatusUpdate();
  }

  if ((millis() - updateCheckTimer) >= updateCheckInterval)
  { // Run periodic update check
    updateCheckTimer = millis();
    if (updateCheck())
    { // Send a status update if the update check worked
      statusUpdateTimer = millis();
      mqttStatusUpdate();
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

  if (beepEnabled)
  { // Process Beeps
    if ((beepState == true) && (millis() - beepTimer >= beepOnTime) && ((beepCounter > 0)))
    {
      beepState = false;         // Turn it off
      beepTimer = millis(); // Remember the time
      analogWrite(beepPin, 254); // start beep for beepOnTime
      if (beepCounter > 0)
      { // Update the beep counter.
        beepCounter--;
      }
    }
    else if ((beepState == false) && (millis() - beepTimer >= beepOffTime) && ((beepCounter >= 0)))
    {
      beepState = true;          // turn it on
      beepTimer = millis(); // Remember the time
      analogWrite(beepPin, 0);   // stop beep for beepOffTime
    }
  }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttConnect()
{ // MQTT connection and subscriptions

  static bool mqttFirstConnect = true; // For the first connection, we want to send an OFF/ON state to
                                       // trigger any automations, but skip that if we reconnect while
                                       // still running the sketch

  // Check to see if we have a broker configured and notify the user if not
  if (mqttServer[0] == 0)
  {
    nextion.SendCmd("page 0");
    nextion.SetAttr("p[0].b[1].font", "6");
    nextion.SetAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rConfigure MQTT:\\rhttp://" + WiFi.localIP().toString() + "\"");
    while (mqttServer[0] == 0)
    { // Handle HTTP and OTA while we're waiting for MQTT to be configured
      yield();
      if (nextion.HandleInput())
      { // Process user input from HMI
        nextion.ProcessInput();
      }
      webServer.handleClient();
      ArduinoOTA.handle();
    }
  }
  // MQTT topic string definitions
  mqttStateTopic = "hasp/" + String(haspNode) + "/state";
  mqttStateJSONTopic = "hasp/" + String(haspNode) + "/state/json";
  mqttCommandTopic = "hasp/" + String(haspNode) + "/command";
  mqttGroupCommandTopic = "hasp/" + String(groupName) + "/command";
  mqttStatusTopic = "hasp/" + String(haspNode) + "/status";
  mqttSensorTopic = "hasp/" + String(haspNode) + "/sensor";
  mqttLightCommandTopic = "hasp/" + String(haspNode) + "/light/switch";
  mqttLightStateTopic = "hasp/" + String(haspNode) + "/light/state";
  mqttLightBrightCommandTopic = "hasp/" + String(haspNode) + "/brightness/set";
  mqttLightBrightStateTopic = "hasp/" + String(haspNode) + "/brightness/state";
  mqttMotionStateTopic = "hasp/" + String(haspNode) + "/motion/state";

  const String mqttCommandSubscription = mqttCommandTopic + "/#";
  const String mqttGroupCommandSubscription = mqttGroupCommandTopic + "/#";
  const String mqttLightSubscription = "hasp/" + String(haspNode) + "/light/#";
  const String mqttLightBrightSubscription = "hasp/" + String(haspNode) + "/brightness/#";

  // Loop until we're reconnected to MQTT
  while (!mqttClient.connected())
  {
    // Create a reconnect counter
    static uint8_t mqttReconnectCount = 0;

    // Generate an MQTT client ID as haspNode + our MAC address
    mqttClientId = String(haspNode) + "-" + String(espMac[0], HEX) + String(espMac[1], HEX) + String(espMac[2], HEX) + String(espMac[3], HEX) + String(espMac[4], HEX) + String(espMac[5], HEX);
    nextion.SendCmd("page 0");
    nextion.SetAttr("p[0].b[1].font", "6");
    nextion.SetAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rMQTT Connecting:\\r " + String(mqttServer) + "\"");
    debug.printLn(String(F("MQTT: Attempting connection to broker ")) + String(mqttServer) + " as clientID " + mqttClientId);

    // Set keepAlive, cleanSession, timeout
    mqttClient.setOptions(30, true, 5000);

    // declare LWT
    mqttClient.setWill(mqttStatusTopic.c_str(), "OFF");

    if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword))
    { // Attempt to connect to broker, setting last will and testament
      // Subscribe to our incoming topics
      if (mqttClient.subscribe(mqttCommandSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + mqttCommandSubscription);
      }
      if (mqttClient.subscribe(mqttGroupCommandSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + mqttGroupCommandSubscription);
      }
      if (mqttClient.subscribe(mqttLightSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }
      if (mqttClient.subscribe(mqttLightBrightSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }
      if (mqttClient.subscribe(mqttStatusTopic))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + mqttStatusTopic);
      }

      if (mqttFirstConnect)
      { // Force any subscribed clients to toggle OFF/ON when we first connect to
        // make sure we get a full panel refresh at power on.  Sending OFF,
        // "ON" will be sent by the mqttStatusTopic subscription action.
        debug.printLn(String(F("MQTT: binary_sensor state: [")) + mqttStatusTopic + "] : [OFF]");
        mqttClient.publish(mqttStatusTopic, "OFF", true, 1);
        mqttFirstConnect = false;
      }
      else
      {
        debug.printLn(String(F("MQTT: binary_sensor state: [")) + mqttStatusTopic + "] : [ON]");
        mqttClient.publish(mqttStatusTopic, "ON", true, 1);
      }

      mqttReconnectCount = 0;

      // Update panel with MQTT status
      nextion.SetAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rMQTT Connected:\\r " + String(mqttServer) + "\"");
      debug.printLn(F("MQTT: connected"));
      if (nextion.getActivePage())
      {
        nextion.SendCmd("page " + String(nextion.getActivePage()));
      }
    }
    else
    { // Retry until we give up and restart after connectTimeout seconds
      mqttReconnectCount++;
      if (mqttReconnectCount > ((connectTimeout / 10) - 1))
      {
        debug.printLn(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Restarting device.")));
        espReset();
      }
      debug.printLn(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Trying again in 30 seconds.")));
      nextion.SetAttr("p[0].b[1].txt", "\"WiFi Connected:\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rMQTT Connect to:\\r " + String(mqttServer) + "\\rFAILED rc=" + String(mqttClient.returnCode()) + "\\r\\rRetry in 30 sec\"");
      uint32_t mqttReconnectTimer = millis(); // record current time for our timeout
      while ((millis() - mqttReconnectTimer) < 30000)
      { // Handle HTTP and OTA while we're waiting 30sec for MQTT to reconnect
        if (nextion.HandleInput())
        { // Process user input from HMI
          nextion.ProcessInput();
        }
        webServer.handleClient();
        ArduinoOTA.handle();
        delay(10);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttCallback(String &strTopic, String &strPayload)
{ // Handle incoming commands from MQTT

  // strTopic: homeassistant/haswitchplate/devicename/command/p[1].b[4].txt
  // strPayload: "Lights On"
  // subTopic: p[1].b[4].txt

  // Incoming Namespace (replace /device/ with /group/ for group commands)
  // '[...]/device/command' -m '' = No command requested, respond with mqttStatusUpdate()
  // '[...]/device/command' -m 'dim=50' = nextion.SendCmd("dim=50")
  // '[...]/device/command/json' -m '["dim=5", "page 1"]' = nextion.SendCmd("dim=50"), nextion.SendCmd("page 1")
  // '[...]/device/command/page' -m '1' = nextion.SendCmd("page 1")
  // '[...]/device/command/statusupdate' -m '' = mqttStatusUpdate()
  // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = nextion.StartOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
  // '[...]/device/command/lcdupdate' -m '' = nextion.StartOtaDownload("lcdFirmwareUrl")
  // '[...]/device/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' = espStartOta("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
  // '[...]/device/command/espupdate' -m '' = espStartOta("espFirmwareUrl")
  // '[...]/device/command/p[1].b[4].txt' -m '' = nextion.GetAttr("p[1].b[4].txt")
  // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = nextion.SetAttr("p[1].b[4].txt", "\"Lights On\"")

  debug.printLn(MQTT, String(F("MQTT IN: '")) + strTopic + "' : '" + strPayload + "'");

  if (((strTopic == mqttCommandTopic) || (strTopic == mqttGroupCommandTopic)) && (strPayload == ""))
  {                     // '[...]/device/command' -m '' = No command requested, respond with mqttStatusUpdate()
    mqttStatusUpdate(); // return status JSON via MQTT
  }
  else if (strTopic == mqttCommandTopic || strTopic == mqttGroupCommandTopic)
  { // '[...]/device/command' -m 'dim=50' == nextion.SendCmd("dim=50")
    nextion.SendCmd(strPayload);
  }
  else if (strTopic == (mqttCommandTopic + "/page") || strTopic == (mqttGroupCommandTopic + "/page"))
  { // '[...]/device/command/page' -m '1' == nextion.SendCmd("page 1")
    nextion.changePage(strPayload.toInt());
  }
  else if (strTopic == (mqttCommandTopic + "/globalpage") || strTopic == (mqttGroupCommandTopic + "/globalpage"))
  { // '[...]/device/command/globalpage' -m '1' sets pageIsGlobal flag
    if( strPayload == "" )
    {
      ; // eh, what to do with an empty payload?
    }
    else
    { // could tokenise with commas using subtring?
      nextion.setPageGlobal(strPayload.toInt(),true);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/localpage") || strTopic == (mqttGroupCommandTopic + "/localpage"))
  { // '[...]/device/command/globalpage' -m '1' sets pageIsGlobal flag
    if( strPayload == "" )
    {
      ; // eh, what to do with an empty payload?
    }
    else
    { // could tokenise with commas using subtring?
      nextion.setPageGlobal(strPayload.toInt(),false);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/json") || strTopic == (mqttGroupCommandTopic + "/json"))
  {                               // '[...]/device/command/json' -m '["dim=5", "page 1"]' = nextion.SendCmd("dim=50"), nextion.SendCmd("page 1")
    nextion.ParseJson(strPayload); // Send to nextion.ParseJson()
  }
  else if (strTopic == (mqttCommandTopic + "/statusupdate") || strTopic == (mqttGroupCommandTopic + "/statusupdate"))
  {                     // '[...]/device/command/statusupdate' == mqttStatusUpdate()
    mqttStatusUpdate(); // return status JSON via MQTT
  }
  else if (strTopic == (mqttCommandTopic + "/lcdupdate") || strTopic == (mqttGroupCommandTopic + "/lcdupdate"))
  { // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' == nextion.StartOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
    if (strPayload == "")
    {
      nextion.StartOtaDownload(lcdFirmwareUrl);
    }
    else
    {
      nextion.StartOtaDownload(strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/espupdate") || strTopic == (mqttGroupCommandTopic + "/espupdate"))
  { // '[...]/device/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' == espStartOta("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
    if (strPayload == "")
    {
      espStartOta(espFirmwareUrl);
    }
    else
    {
      espStartOta(strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/reboot") || strTopic == (mqttGroupCommandTopic + "/reboot"))
  { // '[...]/device/command/reboot' == reboot microcontroller)
    debug.printLn(F("MQTT: Rebooting device"));
    espReset();
  }
  else if (strTopic == (mqttCommandTopic + "/lcdreboot") || strTopic == (mqttGroupCommandTopic + "/lcdreboot"))
  { // '[...]/device/command/lcdreboot' == reboot LCD panel)
    debug.printLn(F("MQTT: Rebooting LCD"));
    nextion.Reset();
  }
  else if (strTopic == (mqttCommandTopic + "/factoryreset") || strTopic == (mqttGroupCommandTopic + "/factoryreset"))
  { // '[...]/device/command/factoryreset' == clear all saved settings)
    configClearSaved();
  }
  else if (strTopic == (mqttCommandTopic + "/beep") || strTopic == (mqttGroupCommandTopic + "/beep"))
  { // '[...]/device/command/beep')
    String mqqtvar1 = getSubtringField(strPayload, ',', 0);
    String mqqtvar2 = getSubtringField(strPayload, ',', 1);
    String mqqtvar3 = getSubtringField(strPayload, ',', 2);

    beepOnTime = mqqtvar1.toInt();
    beepOffTime = mqqtvar2.toInt();
    beepCounter = mqqtvar3.toInt();
  }
  else if (strTopic.startsWith(mqttCommandTopic) && (strPayload == ""))
  { // '[...]/device/command/p[1].b[4].txt' -m '' == nextion.GetAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    nextion.GetAttr(subTopic);
  }
  else if (strTopic.startsWith(mqttGroupCommandTopic) && (strPayload == ""))
  { // '[...]/group/command/p[1].b[4].txt' -m '' == nextion.GetAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(mqttGroupCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    nextion.GetAttr(subTopic);
  }
  else if (strTopic.startsWith(mqttCommandTopic))
  { // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' == nextion.SetAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    nextion.SetAttr(subTopic, strPayload);
  }
  else if (strTopic.startsWith(mqttGroupCommandTopic))
  { // '[...]/group/command/p[1].b[4].txt' -m '"Lights On"' == nextion.SetAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(mqttGroupCommandTopic.length() + 1);
    nextion.SetAttr(subTopic, strPayload);
  }
  else if (strTopic == mqttLightBrightCommandTopic)
  { // change the brightness from the light topic
    int panelDim = map(strPayload.toInt(), 0, 255, 0, 100);
    nextion.SetAttr("dim", String(panelDim));
    nextion.SendCmd("dims=dim");
    mqttClient.publish(mqttLightBrightStateTopic, strPayload);
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "OFF")
  { // set the panel dim OFF from the light topic, saving current dim level first
    nextion.SendCmd("dims=dim");
    nextion.SetAttr("dim", "0");
    mqttClient.publish(mqttLightStateTopic, "OFF");
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "ON")
  { // set the panel dim ON from the light topic, restoring saved dim level
    nextion.SendCmd("dim=dims");
    mqttClient.publish(mqttLightStateTopic, "ON");
  }
  else if (strTopic == mqttStatusTopic && strPayload == "OFF")
  { // catch a dangling LWT from a previous connection if it appears
    mqttClient.publish(mqttStatusTopic, "ON");
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttStatusUpdate()
{ // Periodically publish a JSON string indicating system status
  String mqttStatusPayload = "{";
  mqttStatusPayload += String(F("\"status\":\"available\","));
  mqttStatusPayload += String(F("\"espVersion\":")) + String(haspVersion) + String(F(","));
  if (updateEspAvailable)
  {
    mqttStatusPayload += String(F("\"updateEspAvailable\":true,"));
  }
  else
  {
    mqttStatusPayload += String(F("\"updateEspAvailable\":false,"));
  }
  if (nextion.getLCDConnected())
  {
    mqttStatusPayload += String(F("\"lcdConnected\":true,"));
  }
  else
  {
    mqttStatusPayload += String(F("\"lcdConnected\":false,"));
  }
  mqttStatusPayload += String(F("\"lcdVersion\":\"")) + String(nextion.getLCDVersion()) + String(F("\","));
  if (updateLcdAvailable)
  {
    mqttStatusPayload += String(F("\"updateLcdAvailable\":true,"));
  }
  else
  {
    mqttStatusPayload += String(F("\"updateLcdAvailable\":false,"));
  }
  mqttStatusPayload += String(F("\"espUptime\":")) + String(int32_t(millis() / 1000)) + String(F(","));
  mqttStatusPayload += String(F("\"signalStrength\":")) + String(WiFi.RSSI()) + String(F(","));
  mqttStatusPayload += String(F("\"haspIP\":\"")) + WiFi.localIP().toString() + String(F("\","));
  mqttStatusPayload += String(F("\"heapFree\":")) + String(ESP.getFreeHeap()) + String(F(","));
  mqttStatusPayload += String(F("\"heapFragmentation\":")) + String(ESP.getHeapFragmentation()) + String(F(","));
  mqttStatusPayload += String(F("\"espCore\":\"")) + String(ESP.getCoreVersion()) + String(F("\""));
  mqttStatusPayload += "}";

  mqttClient.publish(mqttSensorTopic, mqttStatusPayload, true, 1);
  mqttClient.publish(mqttStatusTopic, "ON", true, 1);
  debug.printLn(String(F("MQTT: status update: ")) + String(mqttStatusPayload));
  debug.printLn(String(F("MQTT: binary_sensor state: [")) + mqttStatusTopic + "] : [ON]");
  nextion.debug_page_cache();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiSetup()
{ // Connect to WiFi
  nextion.SendCmd("page 0");
  nextion.SetAttr("p[0].b[1].font", "6");
  nextion.SetAttr("p[0].b[1].txt", "\"WiFi Connecting...\\r " + String(WiFi.SSID()) + "\"");

  WiFi.macAddress(espMac);            // Read our MAC address and save it to espMac
  WiFi.hostname(haspNode);            // Assign our hostname before connecting to WiFi
  WiFi.setAutoReconnect(true);        // Tell WiFi to autoreconnect if connection has dropped
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable WiFi sleep modes to prevent occasional disconnects

  if (String(wifiSSID) == "")
  { // If the sketch has not defined a static wifiSSID use WiFiManager to collect required information from the user.

    // id/name, placeholder/prompt, default value, length, extra tags
    WiFiManagerParameter custom_haspNodeHeader("<br/><br/><b>HASP Node Name</b>");
    WiFiManagerParameter custom_haspNode("haspNode", "HASP Node (required. lowercase letters, numbers, and _ only)", haspNode, 15, " maxlength=15 required pattern='[a-z0-9_]*'");
    WiFiManagerParameter custom_groupName("groupName", "Group Name (required)", groupName, 15, " maxlength=15 required");
    WiFiManagerParameter custom_mqttHeader("<br/><br/><b>MQTT Broker</b>");
    WiFiManagerParameter custom_mqttServer("mqttServer", "MQTT Server", mqttServer, 63, " maxlength=39");
    WiFiManagerParameter custom_mqttPort("mqttPort", "MQTT Port", mqttPort, 5, " maxlength=5 type='number'");
    WiFiManagerParameter custom_mqttUser("mqttUser", "MQTT User", mqttUser, 31, " maxlength=31");
    WiFiManagerParameter custom_mqttPassword("mqttPassword", "MQTT Password", mqttPassword, 31, " maxlength=31 type='password'");
    WiFiManagerParameter custom_configHeader("<br/><br/><b>Admin access</b>");
    WiFiManagerParameter custom_configUser("configUser", "Config User", configUser, 15, " maxlength=31'");
    WiFiManagerParameter custom_configPassword("configPassword", "Config Password", configPassword, 31, " maxlength=31 type='password'");

    WiFiManager wifiManager;
    wifiManager.setSaveConfigCallback(configSaveCallback); // set config save notify callback
    wifiManager.setCustomHeadElement(HASP_STYLE);          // add custom style
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
      debug.printLn(F("WIFI: Failed to connect and hit timeout"));
      espReset();
    }

    // Read updated parameters
    strcpy(mqttServer, custom_mqttServer.getValue());
    strcpy(mqttPort, custom_mqttPort.getValue());
    strcpy(mqttUser, custom_mqttUser.getValue());
    strcpy(mqttPassword, custom_mqttPassword.getValue());
    strcpy(haspNode, custom_haspNode.getValue());
    strcpy(groupName, custom_groupName.getValue());
    strcpy(configUser, custom_configUser.getValue());
    strcpy(configPassword, custom_configPassword.getValue());

    if (shouldSaveConfig)
    { // Save the custom parameters to FS
      configSave();
    }
  }
  else
  { // wifiSSID has been defined, so attempt to connect to it forever
    debug.printLn(String(F("Connecting to WiFi network: ")) + String(wifiSSID));
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPass);

    uint32_t wifiReconnectTimer = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      if (millis() >= (wifiReconnectTimer + (connectTimeout * ASECOND)))
      { // If we've been trying to reconnect for connectTimeout seconds, reboot and try again
        debug.printLn(F("WIFI: Failed to connect and hit timeout"));
        espReset();
      }
    }
  }
  // If you get here you have connected to WiFi
  nextion.SetAttr("p[0].b[1].font", "6");
  nextion.SetAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\"");
  debug.printLn(String(F("WIFI: Connected successfully and assigned IP: ")) + WiFi.localIP().toString());
  if (nextion.getActivePage())
  {
    nextion.SendCmd("page " + String(nextion.getActivePage()));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiReconnect()
{ // Existing WiFi connection dropped, try to reconnect
  debug.printLn(F("Reconnecting to WiFi network..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPass);

  uint32_t wifiReconnectTimer = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (millis() >= (wifiReconnectTimer + (reConnectTimeout * ASECOND)))
    { // If we've been trying to reconnect for reConnectTimeout seconds, reboot and try again
      debug.printLn(F("WIFI: Failed to reconnect and hit timeout"));
      espReset();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiConfigCallback(WiFiManager *myWiFiManager)
{ // Notify the user that we're entering config mode
  debug.printLn(F("WIFI: Failed to connect to assigned AP, entering config mode"));
  while (millis() < 800)
  { // for factory-reset system this will be called before display is responsive. give it a second.
    delay(10);
  }
  nextion.SendCmd("page 0");
  nextion.SetAttr("p[0].b[1].font", "6");
  nextion.SetAttr("p[0].b[1].txt", "\" HASP WiFi Setup\\r AP: " + String(wifiConfigAP) + "\\rPassword: " + String(wifiConfigPass) + "\\r\\r\\r\\r\\r\\r\\r  http://192.168.4.1\"");
  nextion.SendCmd("vis 3,1");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espSetupOta()
{ // (mostly) boilerplate OTA setup from library examples

  ArduinoOTA.setHostname(haspNode);
  ArduinoOTA.setPassword(configPassword);

  ArduinoOTA.onStart([]() {
    debug.printLn(F("ESP OTA: update start"));
    nextion.SendCmd("page 0");
    nextion.SetAttr("p[0].b[1].txt", "\"ESP OTA Update\"");
  });
  ArduinoOTA.onEnd([]() {
    nextion.SendCmd("page 0");
    debug.printLn(F("ESP OTA: update complete"));
    nextion.SetAttr("p[0].b[1].txt", "\"ESP OTA Update\\rComplete!\"");
    espReset();
  });
  ArduinoOTA.onProgress([](uint32_t progress, uint32_t total) {
    nextion.SetAttr("p[0].b[1].txt", "\"ESP OTA Update\\rProgress: " + String(progress / (total / 100)) + "%\"");
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
    nextion.SetAttr("p[0].b[1].txt", "\"ESP OTA FAILED\"");
    delay(5000);
    nextion.SendCmd("page " + String(nextion.getActivePage()));
  });
  ArduinoOTA.begin();
  debug.printLn(F("ESP OTA: Over the Air firmware update ready"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espStartOta(String espOtaUrl)
{ // Update ESP firmware from HTTP
  nextion.SendCmd("page 0");
  nextion.SetAttr("p[0].b[1].txt", "\"HTTP update\\rstarting...\"");
  WiFiUDP::stopAll(); // Keep mDNS responder from breaking things

  t_httpUpdate_return returnCode = ESPhttpUpdate.update(wifiClient, espOtaUrl);
  switch (returnCode)
  {
  case HTTP_UPDATE_FAILED:
    debug.printLn("ESPFW: HTTP_UPDATE_FAILED error " + String(ESPhttpUpdate.getLastError()) + " " + ESPhttpUpdate.getLastErrorString());
    nextion.SetAttr("p[0].b[1].txt", "\"HTTP Update\\rFAILED\"");
    break;

  case HTTP_UPDATE_NO_UPDATES:
    debug.printLn(F("ESPFW: HTTP_UPDATE_NO_UPDATES"));
    nextion.SetAttr("p[0].b[1].txt", "\"HTTP Update\\rNo update\"");
    break;

  case HTTP_UPDATE_OK:
    debug.printLn(F("ESPFW: HTTP_UPDATE_OK"));
    nextion.SetAttr("p[0].b[1].txt", "\"HTTP Update\\rcomplete!\\r\\rRestarting.\"");
    espReset();
  }
  delay(5000);
  nextion.SendCmd("page " + String(nextion.getActivePage()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espReset()
{
  debug.printLn(F("RESET: HASP reset"));
  if (mqttClient.connected())
  {
    mqttClient.publish(mqttStatusTopic, "OFF", true, 1);
    mqttClient.publish(mqttSensorTopic, "{\"status\": \"unavailable\"}", true, 1);
    mqttClient.disconnect();
  }
  nextion.Reset();
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
            strcpy(mqttServer, configJson["mqttServer"]);
          }
          if (!configJson["mqttPort"].isNull())
          {
            strcpy(mqttPort, configJson["mqttPort"]);
          }
          if (!configJson["mqttUser"].isNull())
          {
            strcpy(mqttUser, configJson["mqttUser"]);
          }
          if (!configJson["mqttPassword"].isNull())
          {
            strcpy(mqttPassword, configJson["mqttPassword"]);
          }
          if (!configJson["haspNode"].isNull())
          {
            strcpy(haspNode, configJson["haspNode"]);
          }
          if (!configJson["groupName"].isNull())
          {
            strcpy(groupName, configJson["groupName"]);
          }
          if (!configJson["configUser"].isNull())
          {
            strcpy(configUser, configJson["configUser"]);
          }
          if (!configJson["configPassword"].isNull())
          {
            strcpy(configPassword, configJson["configPassword"]);
          }
          if (!configJson["motionPinConfig"].isNull())
          {
            strcpy(motionPinConfig, configJson["motionPinConfig"]);
          }
          if (!configJson["debugSerialEnabled"].isNull())
          {
            if (configJson["debugSerialEnabled"])
            {
              debug.enableSerial(true);
            }
            else
            {
              debug.enableSerial(false);
            }
          }
          if (!configJson["debugTelnetEnabled"].isNull())
          {
            if (configJson["debugTelnetEnabled"])
            {
              debug.enableTelnet(true);
            }
            else
            {
              debug.enableTelnet(false);
            }
          }
          if (!configJson["mdnsEnabled"].isNull())
          {
            if (configJson["mdnsEnabled"])
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
            if (configJson["beepEnabled"])
            {
              beepEnabled = true;
            }
            else
            {
              beepEnabled = false;
            }
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
  nextion.SetAttr("p[0].b[1].txt", "\"Saving\\rconfig\"");
  debug.printLn(F("SPIFFS: Saving config"));
  DynamicJsonDocument jsonConfigValues(1024);
  jsonConfigValues["mqttServer"] = mqttServer;
  jsonConfigValues["mqttPort"] = mqttPort;
  jsonConfigValues["mqttUser"] = mqttUser;
  jsonConfigValues["mqttPassword"] = mqttPassword;
  jsonConfigValues["haspNode"] = haspNode;
  jsonConfigValues["groupName"] = groupName;
  jsonConfigValues["configUser"] = configUser;
  jsonConfigValues["configPassword"] = configPassword;
  jsonConfigValues["motionPinConfig"] = motionPinConfig;
  jsonConfigValues["debugSerialEnabled"] = debug.getSerialEnabled();
  jsonConfigValues["debugTelnetEnabled"] = debug.getTelnetEnabled();
  jsonConfigValues["mdnsEnabled"] = mdnsEnabled;
  jsonConfigValues["beepEnabled"] = beepEnabled;

  debug.printLn(String(F("SPIFFS: mqttServer = ")) + String(mqttServer));
  debug.printLn(String(F("SPIFFS: mqttPort = ")) + String(mqttPort));
  debug.printLn(String(F("SPIFFS: mqttUser = ")) + String(mqttUser));
  debug.printLn(String(F("SPIFFS: mqttPassword = ")) + String(mqttPassword));
  debug.printLn(String(F("SPIFFS: haspNode = ")) + String(haspNode));
  debug.printLn(String(F("SPIFFS: groupName = ")) + String(groupName));
  debug.printLn(String(F("SPIFFS: configUser = ")) + String(configUser));
  debug.printLn(String(F("SPIFFS: configPassword = ")) + String(configPassword));
  debug.printLn(String(F("SPIFFS: motionPinConfig = ")) + String(motionPinConfig));
  debug.printLn(String(F("SPIFFS: debugSerialEnabled = ")) + String(debug.getSerialEnabled()));
  debug.printLn(String(F("SPIFFS: debugTelnetEnabled = ")) + String(debug.getTelnetEnabled()));
  debug.printLn(String(F("SPIFFS: mdnsEnabled = ")) + String(mdnsEnabled));
  debug.printLn(String(F("SPIFFS: beepEnabled = ")) + String(beepEnabled));

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
  nextion.SetAttr("dims", "100");
  nextion.SendCmd("page 0");
  nextion.SetAttr("p[0].b[1].txt", "\"Resetting\\rsystem...\"");
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
  nextion.SetAttr("p[0].b[1].txt", "\"Rebooting\\rsystem...\"");
  debug.printLn(F("RESET: Rebooting device"));
  espReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleNotFound()
{ // webServer 404
  debug.printLn(String(F("HTTP: Sending 404 to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = "File Not Found\n\n";
  httpMessage += "URI: ";
  httpMessage += webServer.uri();
  httpMessage += "\nMethod: ";
  httpMessage += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  httpMessage += "\nArguments: ";
  httpMessage += webServer.args();
  httpMessage += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++)
  {
    httpMessage += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleRoot()
{ // http://plate01/
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending root page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", String(haspNode));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode);
  httpMessage += String(F("</h1>"));

  httpMessage += String(F("<form method='POST' action='saveConfig'>"));
  httpMessage += String(F("<b>WiFi SSID</b> <i><small>(required)</small></i><input id='wifiSSID' required name='wifiSSID' maxlength=32 placeholder='WiFi SSID' value='")) + String(WiFi.SSID()) + "'>";
  httpMessage += String(F("<br/><b>WiFi Password</b> <i><small>(required)</small></i><input id='wifiPass' required name='wifiPass' type='password' maxlength=64 placeholder='WiFi Password' value='")) + String("********") + "'>";
  httpMessage += String(F("<br/><br/><b>HASP Node Name</b> <i><small>(required. lowercase letters, numbers, and _ only)</small></i><input id='haspNode' required name='haspNode' maxlength=15 placeholder='HASP Node Name' pattern='[a-z0-9_]*' value='")) + String(haspNode) + "'>";
  httpMessage += String(F("<br/><br/><b>Group Name</b> <i><small>(required)</small></i><input id='groupName' required name='groupName' maxlength=15 placeholder='Group Name' value='")) + String(groupName) + "'>";
  httpMessage += String(F("<br/><br/><b>MQTT Broker</b> <i><small>(required)</small></i><input id='mqttServer' required name='mqttServer' maxlength=63 placeholder='mqttServer' value='")) + String(mqttServer) + "'>";
  httpMessage += String(F("<br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='mqttPort' value='")) + String(mqttPort) + "'>";
  httpMessage += String(F("<br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='mqttUser' maxlength=31 placeholder='mqttUser' value='")) + String(mqttUser) + "'>";
  httpMessage += String(F("<br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='mqttPassword' name='mqttPassword' type='password' maxlength=31 placeholder='mqttPassword' value='"));
  if (strlen(mqttPassword) != 0)
  {
    httpMessage += String("********");
  }
  httpMessage += String(F("'><br/><br/><b>HASP Admin Username</b> <i><small>(optional)</small></i><input id='configUser' name='configUser' maxlength=31 placeholder='Admin User' value='")) + String(configUser) + "'>";
  httpMessage += String(F("<br/><b>HASP Admin Password</b> <i><small>(optional)</small></i><input id='configPassword' name='configPassword' type='password' maxlength=31 placeholder='Admin User Password' value='"));
  if (strlen(configPassword) != 0)
  {
    httpMessage += String("********");
  }
  httpMessage += String(F("'><br/><hr><b>Motion Sensor Pin:&nbsp;</b><select id='motionPinConfig' name='motionPinConfig'>"));
  httpMessage += String(F("<option value='0'"));
  if (!motionPin)
  {
    httpMessage += String(F(" selected"));
  }
  httpMessage += String(F(">disabled/not installed</option><option value='D0'"));
  if (motionPin == D0)
  {
    httpMessage += String(F(" selected"));
  }
  httpMessage += String(F(">D0</option><option value='D1'"));
  if (motionPin == D1)
  {
    httpMessage += String(F(" selected"));
  }
  httpMessage += String(F(">D1</option></select>"));

  httpMessage += String(F("<br/><b>Serial debug output enabled:</b><input id='debugSerialEnabled' name='debugSerialEnabled' type='checkbox'"));
  if (debug.getSerialEnabled())
  {
    httpMessage += String(F(" checked='checked'"));
  }
  httpMessage += String(F("><br/><b>Telnet debug output enabled:</b><input id='debugTelnetEnabled' name='debugTelnetEnabled' type='checkbox'"));
  if (debug.getTelnetEnabled())
  {
    httpMessage += String(F(" checked='checked'"));
  }
  httpMessage += String(F("><br/><b>mDNS enabled:</b><input id='mdnsEnabled' name='mdnsEnabled' type='checkbox'"));
  if (mdnsEnabled)
  {
    httpMessage += String(F(" checked='checked'"));
  }

  httpMessage += String(F("><br/><b>Keypress beep enabled:</b><input id='beepEnabled' name='beepEnabled' type='checkbox'"));
  if (beepEnabled)
  {
    httpMessage += String(F(" checked='checked'"));
  }

  httpMessage += String(F("><br/><hr><button type='submit'>save settings</button></form>"));

  if (updateEspAvailable)
  {
    httpMessage += String(F("<br/><hr><font color='green'><center><h3>HASP Update available!</h3></center></font>"));
    httpMessage += String(F("<form method='get' action='espfirmware'>"));
    httpMessage += String(F("<input id='espFirmwareURL' type='hidden' name='espFirmware' value='")) + espFirmwareUrl + "'>";
    httpMessage += String(F("<button type='submit'>update HASP to v")) + String(updateEspAvailableVersion) + String(F("</button></form>"));
  }

  httpMessage += String(F("<hr><form method='get' action='firmware'>"));
  httpMessage += String(F("<button type='submit'>update firmware</button></form>"));

  httpMessage += String(F("<hr><form method='get' action='reboot'>"));
  httpMessage += String(F("<button type='submit'>reboot device</button></form>"));

  httpMessage += String(F("<hr><form method='get' action='resetBacklight'>"));
  httpMessage += String(F("<button type='submit'>reset lcd backlight</button></form>"));

  httpMessage += String(F("<hr><form method='get' action='resetConfig'>"));
  httpMessage += String(F("<button type='submit'>factory reset settings</button></form>"));

  httpMessage += String(F("<hr><b>MQTT Status: </b>"));
  if (mqttClient.connected())
  { // Check MQTT connection
    httpMessage += String(F("Connected"));
  }
  else
  {
    httpMessage += String(F("<font color='red'><b>Disconnected</b></font>, return code: ")) + String(mqttClient.returnCode());
  }
  httpMessage += String(F("<br/><b>MQTT ClientID: </b>")) + String(mqttClientId);
  httpMessage += String(F("<br/><b>HASP Version: </b>")) + String(haspVersion);
  httpMessage += String(F("<br/><b>LCD Model: </b>")) + String(nextion.getModel());
  httpMessage += String(F("<br/><b>LCD Version: </b>")) + String(nextion.getLCDVersion());
  httpMessage += String(F("<br/><b>LCD Active Page: </b>")) + String(nextion.getActivePage());
  httpMessage += String(F("<br/><b>CPU Frequency: </b>")) + String(ESP.getCpuFreqMHz()) + String(F("MHz"));
  httpMessage += String(F("<br/><b>Sketch Size: </b>")) + String(ESP.getSketchSize()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Free Sketch Space: </b>")) + String(ESP.getFreeSketchSpace()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Heap Free: </b>")) + String(ESP.getFreeHeap());
  httpMessage += String(F("<br/><b>Heap Fragmentation: </b>")) + String(ESP.getHeapFragmentation());
  httpMessage += String(F("<br/><b>ESP core version: </b>")) + String(ESP.getCoreVersion());
  httpMessage += String(F("<br/><b>IP Address: </b>")) + String(WiFi.localIP().toString());
  httpMessage += String(F("<br/><b>Signal Strength: </b>")) + String(WiFi.RSSI());
  httpMessage += String(F("<br/><b>Uptime: </b>")) + String(int32_t(millis() / 1000));
  httpMessage += String(F("<br/><b>Last reset: </b>")) + String(ESP.getResetInfo());

  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleSaveConfig()
{ // http://plate01/saveConfig
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /saveConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", String(haspNode));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);

  bool shouldSaveWifi = false;
  // Check required values
  if (webServer.arg("wifiSSID") != "" && webServer.arg("wifiSSID") != String(WiFi.SSID()))
  { // Handle WiFi update
    shouldSaveConfig = true;
    shouldSaveWifi = true;
    webServer.arg("wifiSSID").toCharArray(wifiSSID, 32);
    if (webServer.arg("wifiPass") != String("********"))
    {
      webServer.arg("wifiPass").toCharArray(wifiPass, 64);
    }
  }
  if (webServer.arg("mqttServer") != "" && webServer.arg("mqttServer") != String(mqttServer))
  { // Handle mqttServer
    shouldSaveConfig = true;
    webServer.arg("mqttServer").toCharArray(mqttServer, 64);
  }
  if (webServer.arg("mqttPort") != "" && webServer.arg("mqttPort") != String(mqttPort))
  { // Handle mqttPort
    shouldSaveConfig = true;
    webServer.arg("mqttPort").toCharArray(mqttPort, 6);
  }
  if (webServer.arg("haspNode") != "" && webServer.arg("haspNode") != String(haspNode))
  { // Handle haspNode
    shouldSaveConfig = true;
    String lowerHaspNode = webServer.arg("haspNode");
    lowerHaspNode.toLowerCase();
    lowerHaspNode.toCharArray(haspNode, 16);
  }
  if (webServer.arg("groupName") != "" && webServer.arg("groupName") != String(groupName))
  { // Handle groupName
    shouldSaveConfig = true;
    webServer.arg("groupName").toCharArray(groupName, 16);
  }
  // Check optional values
  if (webServer.arg("mqttUser") != String(mqttUser))
  { // Handle mqttUser
    shouldSaveConfig = true;
    webServer.arg("mqttUser").toCharArray(mqttUser, 32);
  }
  if (webServer.arg("mqttPassword") != String("********"))
  { // Handle mqttPassword
    shouldSaveConfig = true;
    webServer.arg("mqttPassword").toCharArray(mqttPassword, 32);
  }
  if (webServer.arg("configUser") != String(configUser))
  { // Handle configUser
    shouldSaveConfig = true;
    webServer.arg("configUser").toCharArray(configUser, 32);
  }
  if (webServer.arg("configPassword") != String("********"))
  { // Handle configPassword
    shouldSaveConfig = true;
    webServer.arg("configPassword").toCharArray(configPassword, 32);
  }
  if (webServer.arg("motionPinConfig") != String(motionPinConfig))
  { // Handle motionPinConfig
    shouldSaveConfig = true;
    webServer.arg("motionPinConfig").toCharArray(motionPinConfig, 3);
  }
  if ((webServer.arg("debugSerialEnabled") == String("on")) && !debug.getSerialEnabled())
  { // debugSerialEnabled was disabled but should now be enabled
    shouldSaveConfig = true;
    debug.enableSerial(true);
  }
  else if ((webServer.arg("debugSerialEnabled") == String("")) && debug.getSerialEnabled())
  { // debugSerialEnabled was enabled but should now be disabled
    shouldSaveConfig = true;
    debug.enableSerial(false);
  }
  if ((webServer.arg("debugTelnetEnabled") == String("on")) && !debug.getTelnetEnabled())
  { // debugTelnetEnabled was disabled but should now be enabled
    shouldSaveConfig = true;
    debug.enableTelnet(true);
  }
  else if ((webServer.arg("debugTelnetEnabled") == String("")) && debug.getTelnetEnabled())
  { // debugTelnetEnabled was enabled but should now be disabled
    shouldSaveConfig = true;
    debug.enableTelnet(false);
  }
  if ((webServer.arg("mdnsEnabled") == String("on")) && !mdnsEnabled)
  { // mdnsEnabled was disabled but should now be enabled
    shouldSaveConfig = true;
    mdnsEnabled = true;
  }
  else if ((webServer.arg("mdnsEnabled") == String("")) && mdnsEnabled)
  { // mdnsEnabled was enabled but should now be disabled
    shouldSaveConfig = true;
    mdnsEnabled = false;
  }
  if ((webServer.arg("beepEnabled") == String("on")) && !beepEnabled)
  { // beepEnabled was disabled but should now be enabled
    shouldSaveConfig = true;
    beepEnabled = true;
  }
  else if ((webServer.arg("beepEnabled") == String("")) && beepEnabled)
  { // beepEnabled was enabled but should now be disabled
    shouldSaveConfig = true;
    beepEnabled = false;
  }

  if (shouldSaveConfig)
  { // Config updated, notify user and trigger write to SPIFFS
    httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
    httpMessage += FPSTR(HTTP_HEADER_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
    httpMessage += String(F("<br/>Saving updated configuration values and restarting device"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);

    configSave();
    if (shouldSaveWifi)
    {
      debug.printLn(String(F("CONFIG: Attempting connection to SSID: ")) + webServer.arg("wifiSSID"));
      espWifiSetup();
    }
    espReset();
  }
  else
  { // No change found, notify user and link back to config page
    httpMessage += String(F("<meta http-equiv='refresh' content='3;url=/' />"));
    httpMessage += FPSTR(HTTP_HEADER_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
    httpMessage += String(F("<br/>No changes found, returning to <a href='/'>home page</a>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleResetConfig()
{ // http://plate01/resetConfig
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /resetConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", String(haspNode));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEADER_END);

  if (webServer.arg("confirm") == "yes")
  { // User has confirmed, so reset everything
    httpMessage += String(F("<h1>"));
    httpMessage += String(haspNode);
    httpMessage += String(F("</h1><b>Resetting all saved settings and restarting device into WiFi AP mode</b>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
    delay(1000);
    configClearSaved();
  }
  else
  {
    httpMessage += String(F("<h1>Warning</h1><b>This process will reset all settings to the default values and restart the device.  You may need to connect to the WiFi AP displayed on the panel to re-configure the device before accessing it again."));
    httpMessage += String(F("<br/><hr><br/><form method='get' action='resetConfig'>"));
    httpMessage += String(F("<br/><br/><button type='submit' name='confirm' value='yes'>reset all settings</button></form>"));
    httpMessage += String(F("<br/><hr><br/><form method='get' action='/'>"));
    httpMessage += String(F("<button type='submit'>return home</button></form>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleResetBacklight()
{ // http://plate01/resetBacklight
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }

  debug.printLn(String(F("HTTP: Sending /resetBacklight page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " HASP backlight reset"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='3;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
  httpMessage += String(F("<br/>Resetting backlight to 100%"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  debug.printLn(F("HTTP: Resetting backlight to 100%"));
  nextion.SetAttr("dims", "100");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleFirmware()
{ // http://plate01/firmware
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /firmware page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F(" firmware</h1>"));

  // Display main firmware page
  // HTTPS Disabled pending resolution of issue: https://github.com/esp8266/Arduino/issues/4696
  // Until then, using a proxy host at http://haswitchplate.com to deliver unsecured firmware images from GitHub
  httpMessage += String(F("<form method='get' action='/espfirmware'>"));
  if (updateEspAvailable)
  {
    httpMessage += String(F("<font color='green'><b>HASP ESP8266 update available!</b></font>"));
  }
  httpMessage += String(F("<br/><b>Update ESP8266 from URL</b><small><i> http only</i></small>"));
  httpMessage += String(F("<br/><input id='espFirmwareURL' name='espFirmware' value='")) + espFirmwareUrl + "'>";
  httpMessage += String(F("<br/><br/><button type='submit'>Update ESP from URL</button></form>"));

  httpMessage += String(F("<br/><form method='POST' action='/update' enctype='multipart/form-data'>"));
  httpMessage += String(F("<b>Update ESP8266 from file</b><input type='file' id='espSelect' name='espSelect' accept='.bin'>"));
  httpMessage += String(F("<br/><br/><button type='submit' id='espUploadSubmit' onclick='ackEspUploadSubmit()'>Update ESP from file</button></form>"));

  httpMessage += String(F("<br/><br/><hr><h1>WARNING!</h1>"));
  httpMessage += String(F("<b>Nextion LCD firmware updates can be risky.</b> If interrupted, the HASP will need to be manually power cycled which might mean a trip to the breaker box. "));
  httpMessage += String(F("After a power cycle, the LCD will display an error message until a successful firmware update has completed.<br/>"));

  httpMessage += String(F("<br/><hr><form method='get' action='lcddownload'>"));
  if (updateLcdAvailable)
  {
    httpMessage += String(F("<font color='green'><b>HASP LCD update available!</b></font>"));
  }
  httpMessage += String(F("<br/><b>Update Nextion LCD from URL</b><small><i> http only</i></small>"));
  httpMessage += String(F("<br/><input id='lcdFirmware' name='lcdFirmware' value='")) + lcdFirmwareUrl + "'>";
  httpMessage += String(F("<br/><br/><button type='submit'>Update LCD from URL</button></form>"));

  httpMessage += String(F("<br/><form method='POST' action='/lcdupload' enctype='multipart/form-data'>"));
  httpMessage += String(F("<br/><b>Update Nextion LCD from file</b><input type='file' id='lcdSelect' name='files[]' accept='.tft'/>"));
  httpMessage += String(F("<br/><br/><button type='submit' id='lcdUploadSubmit' onclick='ackLcdUploadSubmit()'>Update LCD from file</button></form>"));

  // Javascript to collect the filesize of the LCD upload and send it to /tftFileSize
  httpMessage += String(F("<script>function handleLcdFileSelect(evt) {"));
  httpMessage += String(F("var uploadFile = evt.target.files[0];"));
  httpMessage += String(F("document.getElementById('lcdUploadSubmit').innerHTML = 'Upload LCD firmware ' + uploadFile.name;"));
  httpMessage += String(F("var tftFileSize = '/tftFileSize?tftFileSize=' + uploadFile.size;"));
  httpMessage += String(F("var xhttp = new XMLHttpRequest();xhttp.open('GET', tftFileSize, true);xhttp.send();}"));
  httpMessage += String(F("function ackLcdUploadSubmit() {document.getElementById('lcdUploadSubmit').innerHTML = 'Uploading LCD firmware...';}"));
  httpMessage += String(F("function handleEspFileSelect(evt) {var uploadFile = evt.target.files[0];document.getElementById('espUploadSubmit').innerHTML = 'Upload ESP firmware ' + uploadFile.name;}"));
  httpMessage += String(F("function ackEspUploadSubmit() {document.getElementById('espUploadSubmit').innerHTML = 'Uploading ESP firmware...';}"));
  httpMessage += String(F("document.getElementById('lcdSelect').addEventListener('change', handleLcdFileSelect, false);"));
  httpMessage += String(F("document.getElementById('espSelect').addEventListener('change', handleEspFileSelect, false);</script>"));

  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleEspFirmware()
{ // http://plate01/espfirmware
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }

  debug.printLn(String(F("HTTP: Sending /espfirmware page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " ESP update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='60;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode) + " ESP update";
  httpMessage += String(F("</h1>"));
  httpMessage += "<br/>Updating ESP firmware from: " + String(webServer.arg("espFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);

  debug.printLn("ESPFW: Attempting ESP firmware update from: " + String(webServer.arg("espFirmware")));
  espStartOta(webServer.arg("espFirmware"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdUpload()
{ // http://plate01/lcdupload
  // Upload firmware to the Nextion LCD via HTTP upload

  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }

  static uint32_t lcdOtaTransferred = 0;
  static uint32_t lcdOtaRemaining;
  static uint16_t lcdOtaParts;
  const uint32_t lcdOtaTimeout = 30000; // timeout for receiving new data in milliseconds
  static uint32_t lcdOtaTimer = 0;      // timer for upload timeout

  HTTPUpload &upload = webServer.upload();

  if (tftFileSize == 0)
  {
    debug.printLn(String(F("LCD OTA: FAILED, no filesize sent.")));
    String httpMessage = FPSTR(HTTP_HEADER);
    httpMessage.replace("{v}", (String(haspNode) + " LCD update"));
    httpMessage += FPSTR(HTTP_SCRIPT);
    httpMessage += FPSTR(HTTP_STYLE);
    httpMessage += String(HASP_STYLE);
    httpMessage += String(F("<meta http-equiv='refresh' content='5;url=/' />"));
    httpMessage += FPSTR(HTTP_HEADER_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + " LCD update FAILED</h1>";
    httpMessage += String(F("No update file size reported.  You must use a modern browser with Javascript enabled."));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
  else if ((lcdOtaTimer > 0) && ((millis() - lcdOtaTimer) > lcdOtaTimeout))
  { // Our timer expired so reset
    debug.printLn(F("LCD OTA: ERROR: LCD upload timeout.  Restarting."));
    espReset();
  }
  else if (upload.status == UPLOAD_FILE_START)
  {
    WiFiUDP::stopAll(); // Keep mDNS responder from breaking things

    debug.printLn(String(F("LCD OTA: Attempting firmware upload")));
    debug.printLn(String(F("LCD OTA: upload.filename: ")) + String(upload.filename));
    debug.printLn(String(F("LCD OTA: TFTfileSize: ")) + String(tftFileSize));

    lcdOtaRemaining = tftFileSize;
    lcdOtaParts = (lcdOtaRemaining / 4096) + 1;
    debug.printLn(String(F("LCD OTA: File upload beginning. Size ")) + String(lcdOtaRemaining) + String(F(" bytes in ")) + String(lcdOtaParts) + String(F(" 4k chunks.")));

    Serial1.write(nextion.Suffix, sizeof(nextion.Suffix)); // Send empty command to LCD
    Serial1.flush();
    nextion.HandleInput();

    String lcdOtaNextionCmd = "whmi-wri " + String(tftFileSize) + ",115200,0";
    debug.printLn(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
    Serial1.print(lcdOtaNextionCmd);
    Serial1.write(nextion.Suffix, sizeof(nextion.Suffix));
    Serial1.flush();

    if (nextion.OtaResponse())
    {
      debug.printLn(F("LCD OTA: LCD upload command accepted"));
    }
    else
    {
      debug.printLn(F("LCD OTA: LCD upload command FAILED."));
      espReset();
    }
    lcdOtaTimer = millis();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  { // Handle upload data
    static int lcdOtaChunkCounter = 0;
    static uint16_t lcdOtaPartNum = 0;
    static int lcdOtaPercentComplete = 0;
    static const uint16_t lcdOtaBufferSize = 1024; // upload data buffer before sending to UART
    static uint8_t lcdOtaBuffer[lcdOtaBufferSize] = {};
    uint16_t lcdOtaUploadIndex = 0;
    int32_t lcdOtaPacketRemaining = upload.currentSize;

    while (lcdOtaPacketRemaining > 0)
    { // Write incoming data to panel as it arrives
      // determine chunk size as lowest value of lcdOtaPacketRemaining, lcdOtaBufferSize, or 4096 - lcdOtaChunkCounter
      uint16_t lcdOtaChunkSize = 0;
      if ((lcdOtaPacketRemaining <= lcdOtaBufferSize) && (lcdOtaPacketRemaining <= (4096 - lcdOtaChunkCounter)))
      {
        lcdOtaChunkSize = lcdOtaPacketRemaining;
      }
      else if ((lcdOtaBufferSize <= lcdOtaPacketRemaining) && (lcdOtaBufferSize <= (4096 - lcdOtaChunkCounter)))
      {
        lcdOtaChunkSize = lcdOtaBufferSize;
      }
      else
      {
        lcdOtaChunkSize = 4096 - lcdOtaChunkCounter;
      }

      for (uint16_t i = 0; i < lcdOtaChunkSize; i++)
      { // Load up the UART buffer
        lcdOtaBuffer[i] = upload.buf[lcdOtaUploadIndex];
        lcdOtaUploadIndex++;
      }
      Serial1.flush();                              // Clear out current UART buffer
      Serial1.write(lcdOtaBuffer, lcdOtaChunkSize); // And send the most recent data
      lcdOtaChunkCounter += lcdOtaChunkSize;
      lcdOtaTransferred += lcdOtaChunkSize;
      if (lcdOtaChunkCounter >= 4096)
      {
        Serial1.flush();
        lcdOtaPartNum++;
        lcdOtaPercentComplete = (lcdOtaTransferred * 100) / tftFileSize;
        lcdOtaChunkCounter = 0;
        if (nextion.OtaResponse())
        {
          debug.printLn(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" OK, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
        }
        else
        {
          debug.printLn(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" FAILED, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
        }
      }
      else
      {
        delay(10);
      }
      if (lcdOtaRemaining > 0)
      {
        lcdOtaRemaining -= lcdOtaChunkSize;
      }
      if (lcdOtaPacketRemaining > 0)
      {
        lcdOtaPacketRemaining -= lcdOtaChunkSize;
      }
    }

    if (lcdOtaTransferred >= tftFileSize)
    {
      if (nextion.OtaResponse())
      {
        debug.printLn(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        webServer.sendHeader("Location", "/lcdOtaSuccess");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 5000)
        { // extra 5sec delay while the LCD handles any local firmware updates from new versions of code sent to it
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
      else
      {
        debug.printLn(F("LCD OTA: Failure"));
        webServer.sendHeader("Location", "/lcdOtaFailure");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 1000)
        { // extra 1sec delay for client to grab failure page
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
    }
    lcdOtaTimer = millis();
  }
  else if (upload.status == UPLOAD_FILE_END)
  { // Upload completed
    if (lcdOtaTransferred >= tftFileSize)
    {
      if (nextion.OtaResponse())
      { // YAY WE DID IT
        debug.printLn(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        webServer.sendHeader("Location", "/lcdOtaSuccess");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 5000)
        { // extra 5sec delay while the LCD handles any local firmware updates from new versions of code sent to it
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
      else
      {
        debug.printLn(F("LCD OTA: Failure"));
        webServer.sendHeader("Location", "/lcdOtaFailure");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 1000)
        { // extra 1sec delay for client to grab failure page
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  { // Something went kablooey
    debug.printLn(F("LCD OTA: ERROR: upload.status returned: UPLOAD_FILE_ABORTED"));
    debug.printLn(F("LCD OTA: Failure"));
    webServer.sendHeader("Location", "/lcdOtaFailure");
    webServer.send(303);
    uint32_t lcdOtaDelay = millis();
    while ((millis() - lcdOtaDelay) < 1000)
    { // extra 1sec delay for client to grab failure page
      webServer.handleClient();
      delay(1);
    }
    espReset();
  }
  else
  { // Something went weird, we should never get here...
    debug.printLn(String(F("LCD OTA: upload.status returned: ")) + String(upload.status));
    debug.printLn(F("LCD OTA: Failure"));
    webServer.sendHeader("Location", "/lcdOtaFailure");
    webServer.send(303);
    uint32_t lcdOtaDelay = millis();
    while ((millis() - lcdOtaDelay) < 1000)
    { // extra 1sec delay for client to grab failure page
      webServer.handleClient();
      delay(1);
    }
    espReset();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdUpdateSuccess()
{ // http://plate01/lcdOtaSuccess
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /lcdOtaSuccess page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " LCD update success"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F(" LCD update success</h1>"));
  httpMessage += String(F("Restarting HASwitchPlate to apply changes..."));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdUpdateFailure()
{ // http://plate01/lcdOtaFailure
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /lcdOtaFailure page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " LCD update failed"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F(" LCD update failed :(</h1>"));
  httpMessage += String(F("Restarting HASwitchPlate to reset device..."));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdDownload()
{ // http://plate01/lcddownload
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /lcddownload page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " LCD update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode) + " LCD update";
  httpMessage += String(F("</h1>"));
  httpMessage += "<br/>Updating LCD firmware from: " + String(webServer.arg("lcdFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);

  nextion.StartOtaDownload(webServer.arg("lcdFirmware"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleTftFileSize()
{ // http://plate01/tftFileSize
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /tftFileSize page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " TFT Filesize"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  tftFileSize = webServer.arg("tftFileSize").toInt();
  debug.printLn(String(F("WEB: tftFileSize: ")) + String(tftFileSize));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleReboot()
{ // http://plate01/reboot
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debug.printLn(String(F("HTTP: Sending /reboot page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(haspNode) + " HASP reboot"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='10;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
  httpMessage += String(F("<br/>Rebooting device"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  debug.printLn(F("RESET: Rebooting device"));
  nextion.SendCmd("page 0");
  nextion.SetAttr("p[0].b[1].txt", "\"Rebooting...\"");
  espReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool updateCheck()
{ // firmware update check
  // Gerard has placed nodes on an isolated network. Connecting to the live internet is verboten and unpossible. So skip the pointless check
  updateEspAvailable = false;
  updateLcdAvailable = false;
  return true;
/*
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
      if (updateEspAvailableVersion > haspVersion)
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
  */
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void motionSetup()
{
  if (strcmp(motionPinConfig, "D0") == 0)
  {
    motionEnabled = true;
    motionPin = D0;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(motionPinConfig, "D1") == 0)
  {
    motionEnabled = true;
    motionPin = D1;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(motionPinConfig, "D2") == 0)
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
      mqttClient.publish(mqttMotionStateTopic, "ON");
      motionActive = motionActiveBuffer;
      debug.printLn("MOTION: Active");
    }
    else if ((!motionActiveBuffer && motionActive) && (millis() > (motionLatchTimer + motionLatchTimeout)))
    {
      motionLatchTimer = millis();
      mqttClient.publish(mqttMotionStateTopic, "OFF");
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
        nextion.SendCmd(String(telnetInputBuffer));
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
