// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// mqtt_class.cpp : Class internals to interact with the distant MQTT Server
//
// ----------------------------------------------------------------------------------------------------------------- //


#include "settings.h"
#include <Arduino.h>
#include <MQTT.h>
#include <ArduinoOTA.h>
#include "mqtt_class.h"

#include "debug.h"
extern debugClass debug; // our serial debug interface

#include "config_class.h"
extern ConfigClass config; // our Configuration Container

#include "hmi_nextion.h" // circular?
extern hmiNextionClass nextion;  // our LCD Object

#include "web_class.h"
extern WebClass web; // our HTTP Object

#include "speaker_class.h"
extern SpeakerClass beep; // our Speaker Object


////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Class These!
extern uint8_t espMac[6];                          // Byte array to store our MAC address
extern String lcdFirmwareUrl;                      // Default link to compiled Nextion firmware images
extern String espFirmwareUrl;                      // Default link to compiled Arduino firmware image
extern bool updateEspAvailable;                    // Flag for update check to report new ESP FW version
extern bool updateLcdAvailable;                    // Flag for update check to report new LCD FW version

// TODO: Class These!
extern String getSubtringField(String data, char separator, int index); // TODO: class me
extern String printHex8(String data, uint8_t length); // TODO: class me
extern bool updateCheck();
extern void espReset();
extern void espStartOta(String espOtaUrl);
extern void configClearSaved();


////////////////////////////////////////////////////////////////////////////////////////////////////
// Our internal objects
// TODO: can these go into our class?

WiFiClient wifiMQTTClient;                 // client for MQTT
MQTTClient mqttClient(mqttMaxPacketSize);  // MQTT Object

extern MQTTClass mqtt;

////////////////////////////////////////////////////////////////////////////////////////////////////
// callback prototype is "typedef void (*MQTTClientCallbackSimple)(String &topic, String &payload)"
// So we cannot declare our callback within the class, as it gets the wrong prototype
// So we have our callback outside the class and then have it call into the (global) class
// to do the actual work of parsing the mqtt message
void mqtt_callback(String &strTopic, String &strPayload)
{
  mqtt.callback(strTopic, strPayload);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::begin()
{ // called in the main code setup, handles our initialisation
  _alive=true;
  _statusUpdateTimer = 0;
  mqttClient.begin(config.getMQTTServer(), atoi(config.getMQTTPort()), wifiMQTTClient); // Create MQTT service object
  mqttClient.onMessage(mqtt_callback);                          // Setup MQTT callback function
  connect();                                                    // Connect to MQTT
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::loop()
{ // called in the main code loop, handles our periodic code
  if( !_alive )
  {
    begin();
  }
  if (!mqttClient.connected())
  { // Check MQTT connection
    debug.printLn("MQTT: not connected, connecting.");
    connect();
  }

  mqttClient.loop();        // MQTT client loop
  if ((millis() - _statusUpdateTimer) >= statusUpdateInterval)
  { // Run periodic status update
    statusUpdate();
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::connect()
{ // MQTT connection and subscriptions

  static bool mqttFirstConnect = true; // For the first connection, we want to send an OFF/ON state to
  // trigger any automations, but skip that if we reconnect while
  // still running the sketch

  // Check to see if we have a broker configured and notify the user if not
  if (config.getMQTTServer()[0] == 0) // this check could be more elegant, eh?
  {
    nextion.sendCmd("page 0");
    nextion.setAttr("p[0].b[1].font", "6");
    nextion.setAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rConfigure MQTT:\\rhttp://" + WiFi.localIP().toString() + "\"");
    while (config.getMQTTServer()[0] == 0)
    { // Handle HTTP and OTA while we're waiting for MQTT to be configured
      yield();
      if (nextion.handleInput())
      { // Process user input from HMI
        nextion.processInput();
      }
      web.loop();
      ArduinoOTA.handle(); // TODO: move this elsewhere!
    }
  }
  // MQTT topic string definitions
  _stateTopic = "hasp/" + String(config.getHaspNode()) + "/state";
  _stateJSONTopic = "hasp/" + String(config.getHaspNode()) + "/state/json";
  _commandTopic = "hasp/" + String(config.getHaspNode()) + "/command";
  _groupCommandTopic = "hasp/" + String(config.getGroupName()) + "/command";
  _statusTopic = "hasp/" + String(config.getHaspNode()) + "/status";
  _sensorTopic = "hasp/" + String(config.getHaspNode()) + "/sensor";
  _lightCommandTopic = "hasp/" + String(config.getHaspNode()) + "/light/switch";
  _lightStateTopic = "hasp/" + String(config.getHaspNode()) + "/light/state";
  _lightBrightCommandTopic = "hasp/" + String(config.getHaspNode()) + "/brightness/set";
  _lightBrightStateTopic = "hasp/" + String(config.getHaspNode()) + "/brightness/state";
  _motionStateTopic = "hasp/" + String(config.getHaspNode()) + "/motion/state";

  const String commandSubscription = _commandTopic + "/#";
  const String groupCommandSubscription = _groupCommandTopic + "/#";
  const String lightSubscription = "hasp/" + String(config.getHaspNode()) + "/light/#";
  const String lightBrightSubscription = "hasp/" + String(config.getHaspNode()) + "/brightness/#";

  // Loop until we're reconnected to MQTT
  while (!mqttClient.connected())
  {
    // Create a reconnect counter
    static uint8_t mqttReconnectCount = 0;

    // Generate an MQTT client ID as haspNode + our MAC address
    _clientId = String(config.getHaspNode()) + "-" + String(espMac[0], HEX) + String(espMac[1], HEX) + String(espMac[2], HEX) + String(espMac[3], HEX) + String(espMac[4], HEX) + String(espMac[5], HEX);
    nextion.sendCmd("page 0");
    nextion.setAttr("p[0].b[1].font", "6");
    nextion.setAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rMQTT Connecting:\\r " + String(config.getMQTTServer()) + "\"");
    debug.printLn(String(F("MQTT: Attempting connection to broker ")) + String(config.getMQTTServer()) + " as clientID " + _clientId);

    // Set keepAlive, cleanSession, timeout
    mqttClient.setOptions(30, true, 5000);

    // declare LWT
    mqttClient.setWill(_statusTopic.c_str(), "OFF");

    if (mqttClient.connect(_clientId.c_str(), config.getMQTTUser(), config.getMQTTPassword()))
    { // Attempt to connect to broker, setting last will and testament
      // Subscribe to our incoming topics
      if (mqttClient.subscribe(commandSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + commandSubscription);
      }
      if (mqttClient.subscribe(groupCommandSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + groupCommandSubscription);
      }
      if (mqttClient.subscribe(lightSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + lightSubscription);
      }
      if (mqttClient.subscribe(lightBrightSubscription))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + lightSubscription);
      }
      if (mqttClient.subscribe(_statusTopic))
      {
        debug.printLn(String(F("MQTT: subscribed to ")) + _statusTopic);
      }

      if (mqttFirstConnect)
      { // Force any subscribed clients to toggle OFF/ON when we first connect to
        // make sure we get a full panel refresh at power on.  Sending OFF,
        // "ON" will be sent by the _statusTopic subscription action.
        debug.printLn(String(F("MQTT: binary_sensor state: [")) + _statusTopic + "] : [OFF]");
        mqttClient.publish(_statusTopic, "OFF", true, 1);
        mqttFirstConnect = false;
      }
      else
      {
        debug.printLn(String(F("MQTT: binary_sensor state: [")) + _statusTopic + "] : [ON]");
        mqttClient.publish(_statusTopic, "ON", true, 1);
      }

      mqttReconnectCount = 0;

      // Update panel with MQTT status
      nextion.setAttr("p[0].b[1].txt", "\"WiFi Connected!\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rMQTT Connected:\\r " + String(config.getMQTTServer()) + "\"");
      debug.printLn(F("MQTT: connected"));
      if (nextion.getActivePage())
      {
        nextion.sendCmd("page " + String(nextion.getActivePage()));
      }
    }
    else
    { // Retry until we give up and restart after mqttConnectTimeout seconds
      mqttReconnectCount++;
      if (mqttReconnectCount > ((mqttConnectTimeout / 10) - 1))
      {
        debug.printLn(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Restarting device.")));
        espReset();
      }
      debug.printLn(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Trying again in 30 seconds.")));
      nextion.setAttr("p[0].b[1].txt", "\"WiFi Connected:\\r " + String(WiFi.SSID()) + "\\rIP: " + WiFi.localIP().toString() + "\\r\\rMQTT Connect to:\\r " + String(config.getMQTTServer()) + "\\rFAILED rc=" + String(mqttClient.returnCode()) + "\\r\\rRetry in 30 sec\"");
      uint32_t mqttReconnectTimer = millis(); // record current time for our timeout
      while ((millis() - mqttReconnectTimer) < 30000)
      { // Handle HTTP and OTA while we're waiting 30sec for MQTT to reconnect
        if (nextion.handleInput())
        { // Process user input from HMI
          nextion.processInput();
        }
        web.loop();
        ArduinoOTA.handle();
        delay(10);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::callback(String &strTopic, String &strPayload)
{ // Handle incoming commands from MQTT

  // strTopic: homeassistant/haswitchplate/devicename/command/p[1].b[4].txt
  // strPayload: "Lights On"
  // subTopic: p[1].b[4].txt

  // Incoming Namespace (replace /device/ with /group/ for group commands)
  // '[...]/device/command' -m '' = No command requested, respond with statusUpdate()
  // '[...]/device/command' -m 'dim=50' = nextion.sendCmd("dim=50")
  // '[...]/device/command/json' -m '["dim=5", "page 1"]' = nextion.sendCmd("dim=50"), nextion.sendCmd("page 1")
  // '[...]/device/command/page' -m '1' = nextion.sendCmd("page 1")
  // '[...]/device/command/statusupdate' -m '' = statusUpdate()
  // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = nextion.startOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
  // '[...]/device/command/lcdupdate' -m '' = nextion.startOtaDownload("lcdFirmwareUrl")
  // '[...]/device/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' = espStartOta("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
  // '[...]/device/command/espupdate' -m '' = espStartOta("espFirmwareUrl")
  // '[...]/device/command/p[1].b[4].txt' -m '' = nextion.getAttr("p[1].b[4].txt")
  // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = nextion.setAttr("p[1].b[4].txt", "\"Lights On\"")

  debug.printLn(MQTT, String(F("MQTT IN: '")) + strTopic + "' : '" + strPayload + "'");

  if (((strTopic == _commandTopic) || (strTopic == _groupCommandTopic)) && (strPayload == ""))
  {                     // '[...]/device/command' -m '' = No command requested, respond with statusUpdate()
    statusUpdate(); // return status JSON via MQTT
  }
  else if (strTopic == _commandTopic || strTopic == _groupCommandTopic)
  { // '[...]/device/command' -m 'dim=50' == nextion.sendCmd("dim=50")
    nextion.sendCmd(strPayload);
  }
  else if (strTopic == (_commandTopic + "/page") || strTopic == (_groupCommandTopic + "/page"))
  { // '[...]/device/command/page' -m '1' == nextion.sendCmd("page 1")
    nextion.changePage(strPayload.toInt());
  }
  else if (strTopic == (_commandTopic + "/globalpage") || strTopic == (_groupCommandTopic + "/globalpage"))
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
  else if (strTopic == (_commandTopic + "/localpage") || strTopic == (_groupCommandTopic + "/localpage"))
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
  else if (strTopic == (_commandTopic + "/json") || strTopic == (_groupCommandTopic + "/json"))
  {                               // '[...]/device/command/json' -m '["dim=5", "page 1"]' = nextion.sendCmd("dim=50"), nextion.sendCmd("page 1")
    nextion.parseJson(strPayload); // Send to nextion.parseJson()
  }
  else if (strTopic == (_commandTopic + "/statusupdate") || strTopic == (_groupCommandTopic + "/statusupdate"))
  {                     // '[...]/device/command/statusupdate' == mqttStatusUpdate()
    statusUpdate(); // return status JSON via MQTT
  }
  else if (strTopic == (_commandTopic + "/lcdupdate") || strTopic == (_groupCommandTopic + "/lcdupdate"))
  { // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' == nextion.startOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
    if (strPayload == "")
    {
      nextion.startOtaDownload(lcdFirmwareUrl);
    }
    else
    {
      nextion.startOtaDownload(strPayload);
    }
  }
  else if (strTopic == (_commandTopic + "/espupdate") || strTopic == (_groupCommandTopic + "/espupdate"))
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
  else if (strTopic == (_commandTopic + "/reboot") || strTopic == (_groupCommandTopic + "/reboot"))
  { // '[...]/device/command/reboot' == reboot microcontroller)
    debug.printLn(F("MQTT: Rebooting device"));
    espReset();
  }
  else if (strTopic == (_commandTopic + "/lcdreboot") || strTopic == (_groupCommandTopic + "/lcdreboot"))
  { // '[...]/device/command/lcdreboot' == reboot LCD panel)
    debug.printLn(F("MQTT: Rebooting LCD"));
    nextion.reset();
  }
  else if (strTopic == (_commandTopic + "/factoryreset") || strTopic == (_groupCommandTopic + "/factoryreset"))
  { // '[...]/device/command/factoryreset' == clear all saved settings)
    configClearSaved();
  }
  else if (strTopic == (_commandTopic + "/beep") || strTopic == (_groupCommandTopic + "/beep"))
  { // '[...]/device/command/beep')
    String mqqtvar1 = getSubtringField(strPayload, ',', 0);
    String mqqtvar2 = getSubtringField(strPayload, ',', 1);
    String mqqtvar3 = getSubtringField(strPayload, ',', 2);
    beep.playSound(mqqtvar1.toInt(), mqqtvar2.toInt(), mqqtvar3.toInt());
  }
  else if (strTopic.startsWith(_commandTopic) && (strPayload == ""))
  { // '[...]/device/command/p[1].b[4].txt' -m '' == nextion.getAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(_commandTopic.length() + 1);
    _getSubtopic = "/" + subTopic;
    nextion.getAttr(subTopic);
  }
  else if (strTopic.startsWith(_groupCommandTopic) && (strPayload == ""))
  { // '[...]/group/command/p[1].b[4].txt' -m '' == nextion.getAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(_groupCommandTopic.length() + 1);
    _getSubtopic = "/" + subTopic;
    nextion.getAttr(subTopic);
  }
  else if (strTopic.startsWith(_commandTopic))
  { // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' == nextion.setAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(_commandTopic.length() + 1);
    nextion.setAttr(subTopic, strPayload);
  }
  else if (strTopic.startsWith(_groupCommandTopic))
  { // '[...]/group/command/p[1].b[4].txt' -m '"Lights On"' == nextion.setAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(_groupCommandTopic.length() + 1);
    nextion.setAttr(subTopic, strPayload);
  }
  else if (strTopic == _lightBrightCommandTopic)
  { // change the brightness from the light topic
    int panelDim = map(strPayload.toInt(), 0, 255, 0, 100);
    nextion.setAttr("dim", String(panelDim));
    nextion.sendCmd("dims=dim");
    mqttClient.publish(_lightBrightStateTopic, strPayload);
  }
  else if (strTopic == _lightCommandTopic && strPayload == "OFF")
  { // set the panel dim OFF from the light topic, saving current dim level first
    nextion.sendCmd("dims=dim");
    nextion.setAttr("dim", "0");
    mqttClient.publish(_lightStateTopic, "OFF");
  }
  else if (strTopic == _lightCommandTopic && strPayload == "ON")
  { // set the panel dim ON from the light topic, restoring saved dim level
    nextion.sendCmd("dim=dims");
    mqttClient.publish(_lightStateTopic, "ON");
  }
  else if (strTopic == _statusTopic && strPayload == "OFF")
  { // catch a dangling LWT from a previous connection if it appears
    mqttClient.publish(_statusTopic, "ON");
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::statusUpdate()
{ // Periodically publish a JSON string indicating system status
  _statusUpdateTimer = millis();
  String statusPayload = "{";
  statusPayload += String(F("\"status\":\"available\","));
  statusPayload += String(F("\"espVersion\":")) + String(config.getHaspVersion()) + String(F(","));
  if (updateEspAvailable)
  {
    statusPayload += String(F("\"updateEspAvailable\":true,"));
  }
  else
  {
    statusPayload += String(F("\"updateEspAvailable\":false,"));
  }
  if (nextion.getLCDConnected())
  {
    statusPayload += String(F("\"lcdConnected\":true,"));
  }
  else
  {
    statusPayload += String(F("\"lcdConnected\":false,"));
  }
  statusPayload += String(F("\"lcdVersion\":\"")) + String(nextion.getLCDVersion()) + String(F("\","));
  if (updateLcdAvailable)
  {
    statusPayload += String(F("\"updateLcdAvailable\":true,"));
  }
  else
  {
    statusPayload += String(F("\"updateLcdAvailable\":false,"));
  }
  statusPayload += String(F("\"espUptime\":")) + String(int32_t(millis() / 1000)) + String(F(","));
  statusPayload += String(F("\"signalStrength\":")) + String(WiFi.RSSI()) + String(F(","));
  statusPayload += String(F("\"haspIP\":\"")) + WiFi.localIP().toString() + String(F("\","));
  statusPayload += String(F("\"heapFree\":")) + String(ESP.getFreeHeap()) + String(F(","));
  statusPayload += String(F("\"heapFragmentation\":")) + String(ESP.getHeapFragmentation()) + String(F(","));
  statusPayload += String(F("\"espCore\":\"")) + String(ESP.getCoreVersion()) + String(F("\""));
  statusPayload += "}";

  mqttClient.publish(_sensorTopic, statusPayload, true, 1);
  mqttClient.publish(_statusTopic, "ON", true, 1);
  debug.printLn(String(F("MQTT: status update: ")) + String(statusPayload));
  debug.printLn(String(F("MQTT: binary_sensor state: [")) + _statusTopic + "] : [ON]");
  nextion.debug_page_cache();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTClass::clientIsConnected() { return mqttClient.connected(); }

////////////////////////////////////////////////////////////////////////////////////////////////////
String MQTTClass::clientReturnCode() { return String(mqttClient.returnCode()); }

////////////////////////////////////////////////////////////////////////////////////////////////////
String MQTTClass::getClientID() { return _clientId; }

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::publishMotionTopic(String msg) { mqttClient.publish(_motionStateTopic, msg); }

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::publishStateTopic(String msg) { mqttClient.publish(_stateTopic, msg); }

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::publishStatusTopic(String msg) { mqttClient.publish(_statusTopic, msg); }

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::publishButtonEvent(String page, String buttonID, String newState)
{ // Publish a message that buttonID on page is now newState
  String mqttButtonTopic = _stateTopic + "/p[" + page + "].b[" + buttonID + "]";
  mqttClient.publish(mqttButtonTopic, newState);
  debug.printLn(MQTT,String(F("MQTT OUT: '")) + mqttButtonTopic + "' : '" + newState + "'");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::publishButtonJSONEvent(String page, String buttonID, String newState)
{ // Publish a JSON message stating button = newState, on the State JSON Topic
  String mqttButtonJSONEvent = String(F("{\"event\":\"p[")) + String(page) + String(F("].b[")) + String(buttonID) + String(F("]\", \"value\":\"")) + newState + String(F("""}"));
  mqttClient.publish(_stateJSONTopic, mqttButtonJSONEvent);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::publishStatePage(String page)
{ // Publish a page message on the State Topic
  String mqttPageTopic = _stateTopic + "/page";
  mqttClient.publish(mqttPageTopic, page);
  debug.printLn(MQTT, String(F("MQTT OUT: '")) + mqttPageTopic + "' : '" + page + "'");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::publishStateSubTopic(String subtopic, String newState)
{ // extend the State Topic with a subtopic and publish a newState message on it
  String mqttReturnTopic = _stateTopic + subtopic;
  mqttClient.publish(mqttReturnTopic, newState);
  debug.printLn(MQTT,String(F("MQTT OUT: '")) + mqttReturnTopic + "' : '" + newState + "]");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTClass::goodbye()
{ // like a Last-Will-and-Testament, publish something when we are going offline
  if (mqttClient.connected())
  {
    mqttClient.publish(_statusTopic, "OFF", true, 1);
    mqttClient.publish(_sensorTopic, "{\"status\": \"unavailable\"}", true, 1);
    mqttClient.disconnect();
  }
}
