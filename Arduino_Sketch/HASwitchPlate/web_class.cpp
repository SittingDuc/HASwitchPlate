// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// mqtt_class.cpp : Class internals to interact with the distant local HTTP server
//
// ----------------------------------------------------------------------------------------------------------------- //


#include "common.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h> // ESP8266HTTPUpdateServer, httpOTAUpdate
#include <WiFiManager.h> // HTTP_HEADER, HTTP_END, etc


extern uint8_t espMac[6];                          // Byte array to store our MAC address
extern String lcdFirmwareUrl;                      // Default link to compiled Nextion firmware images
extern String espFirmwareUrl;                      // Default link to compiled Arduino firmware image
extern bool updateEspAvailable;                    // Flag for update check to report new ESP FW version
extern float updateEspAvailableVersion;            // Float to hold the new ESP FW version number
extern bool updateLcdAvailable;                    // Flag for update check to report new LCD FW version
extern bool shouldSaveConfig;                      // Flag to save json config to SPIFFS
extern uint8_t motionPin;                          // GPIO input pin for motion sensor if connected and enabled
extern uint32_t updateLcdAvailableVersion;         // Int to hold the new LCD FW version number
extern uint32_t tftFileSize;                       // Filesize for TFT firmware upload


ESP8266WebServer webServer(80);            // Server listening for HTTP
ESP8266HTTPUpdateServer httpOTAUpdate;

extern String espFirmwareUrl;
extern String lcdFirmwareUrl;


// a reference to our global copy of self, so we can make working callbacks
extern WebClass web;

////////////////////////////////////////////////////////////////////////////////////////////////////
// callback prototype is "std::function<void ()> handler"
// So we cannot declare our callback within the class, as it gets the wrong prototype
// So we have our callback outside the class and then have it call into the (global) class
// to do the actual work of parsing the http message
// and yes, we need a local copy of "self" to handle our callbacks.
void callback_HandleNotFound()
{
    web._handleNotFound();
}
void callback_HandleRoot()
{
    web._handleRoot();
}
void callback_HandleSaveConfig()
{
    web._handleSaveConfig();
}
void callback_HandleResetConfig()
{
    web._handleResetConfig();
}
void callback_HandleResetBacklight()
{
  web._handleResetBacklight();
}
void callback_HandleFirmware()
{
  web._handleFirmware();
}
void callback_HandleEspFirmware()
{
  web._handleEspFirmware();
}
void callback_HandleLcdUpload()
{
  web._handleLcdUpload();
}
void callback_HandleLcdUpdateSuccess()
{
  web._handleLcdUpdateSuccess();
}
void callback_HandleLcdUpdateFailure()
{
  web._handleLcdUpdateFailure();
}
void callback_HandleLcdDownload()
{
  web._handleLcdDownload();
}
void callback_HandleTftFileSize()
{
  web._handleTftFileSize();
}
void callback_HandleReboot()
{
    web._handleReboot();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::begin()
{ // called in the main code setup, handles our initialisation
  _alive=true;

  setUser(DEFAULT_CONFIG_USER);
  setPassword(DEFAULT_CONFIG_PASS);

  if ((_configPassword[0] != '\0') && (_configUser[0] != '\0'))
  { // Start the webserver with our assigned password if it's been configured...
    httpOTAUpdate.setup(&webServer, "/update", _configUser, _configPassword);
  }
  else
  { // or without a password if not
    httpOTAUpdate.setup(&webServer, "/update");
  }
  webServer.on("/", callback_HandleRoot);
  webServer.on("/saveConfig", callback_HandleSaveConfig);
  webServer.on("/resetConfig", callback_HandleResetConfig);
  webServer.on("/resetBacklight", callback_HandleResetBacklight);
  webServer.on("/firmware", callback_HandleFirmware);
  webServer.on("/espfirmware", callback_HandleEspFirmware);
  webServer.on("/lcdupload", HTTP_POST, []() { webServer.send(200); }, callback_HandleLcdUpload);
  webServer.on("/tftFileSize", callback_HandleTftFileSize);
  webServer.on("/lcddownload", callback_HandleLcdDownload);
  webServer.on("/lcdOtaSuccess", callback_HandleLcdUpdateSuccess);
  webServer.on("/lcdOtaFailure", callback_HandleLcdUpdateFailure);
  webServer.on("/reboot", callback_HandleReboot);
  webServer.onNotFound(callback_HandleNotFound);
  webServer.begin();
  debug.printLn(String(F("HTTP: Server started @ http://")) + WiFi.localIP().toString());

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::loop()
{ // called in the main code loop, handles our periodic code
  if( !_alive )
  {
    begin();
  }
  webServer.handleClient(); // webServer loop
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool WebClass::_authenticated(void)
{ // common code to verify our authentication on most handle callbacks
  if (_configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(_configUser, _configPassword))
    {
      webServer.requestAuthentication();
      return false;
    }
  }
  // authentication passes or not required
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleNotFound()
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
void WebClass::_handleRoot()
{ // http://plate01/
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending root page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", String(config.getHaspNode()));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(config.getHaspNode());
  httpMessage += String(F("</h1>"));

  httpMessage += String(F("<form method='POST' action='saveConfig'>"));
  httpMessage += String(F("<b>WiFi SSID</b> <i><small>(required)</small></i><input id='wifiSSID' required name='wifiSSID' maxlength=32 placeholder='WiFi SSID' value='")) + String(WiFi.SSID()) + "'>";
  httpMessage += String(F("<br/><b>WiFi Password</b> <i><small>(required)</small></i><input id='wifiPass' required name='wifiPass' type='password' maxlength=64 placeholder='WiFi Password' value='")) + String("********") + "'>";
  httpMessage += String(F("<br/><br/><b>HASP Node Name</b> <i><small>(required. lowercase letters, numbers, and _ only)</small></i><input id='haspNode' required name='haspNode' maxlength=15 placeholder='HASP Node Name' pattern='[a-z0-9_]*' value='")) + String(config.getHaspNode()) + "'>";
  httpMessage += String(F("<br/><br/><b>Group Name</b> <i><small>(required)</small></i><input id='groupName' required name='groupName' maxlength=15 placeholder='Group Name' value='")) + String(config.getGroupName()) + "'>";
  httpMessage += String(F("<br/><br/><b>MQTT Broker</b> <i><small>(required)</small></i><input id='mqttServer' required name='mqttServer' maxlength=63 placeholder='mqttServer' value='")) + String(config.getMQTTServer()) + "'>";
  httpMessage += String(F("<br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='mqttPort' value='")) + String(config.getMQTTPort()) + "'>";
  httpMessage += String(F("<br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='mqttUser' maxlength=31 placeholder='mqttUser' value='")) + String(config.getMQTTUser()) + "'>";
  httpMessage += String(F("<br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='mqttPassword' name='mqttPassword' type='password' maxlength=31 placeholder='mqttPassword' value='"));
  if (strlen(config.getMQTTPassword()) != 0)
  {
    httpMessage += String("********");
  }
  httpMessage += String(F("'><br/><br/><b>HASP Admin Username</b> <i><small>(optional)</small></i><input id='configUser' name='configUser' maxlength=31 placeholder='Admin User' value='")) + String(_configUser) + "'>";
  httpMessage += String(F("<br/><b>HASP Admin Password</b> <i><small>(optional)</small></i><input id='configPassword' name='configPassword' type='password' maxlength=31 placeholder='Admin User Password' value='"));
  if (strlen(_configPassword) != 0)
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
  if (config.getMDNSEnabled())
  {
    httpMessage += String(F(" checked='checked'"));
  }

  httpMessage += String(F("><br/><b>Keypress beep enabled:</b><input id='beepEnabled' name='beepEnabled' type='checkbox'"));
  if (beep.getEnable())
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
  if (mqtt.clientIsConnected())
  { // Check MQTT connection
    httpMessage += String(F("Connected"));
  }
  else
  {
    httpMessage += String(F("<font color='red'><b>Disconnected</b></font>, return code: ")) + mqtt.clientReturnCode();
  }
  httpMessage += String(F("<br/><b>MQTT ClientID: </b>")) + mqtt.getClientID();
  httpMessage += String(F("<br/><b>HASP Version: </b>")) + String(config.getHaspVersion());
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
void WebClass::_handleSaveConfig()
{ // http://plate01/saveConfig
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /saveConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", String(config.getHaspNode()));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);

  bool shouldSaveWifi = false;
  // Check required values
  if (webServer.arg("wifiSSID") != "" && webServer.arg("wifiSSID") != String(WiFi.SSID()))
  { // Handle WiFi update
    config.setSaveNeeded();
    shouldSaveWifi = true;
    webServer.arg("wifiSSID").toCharArray(config.getWIFISSID(), 32);
    if (webServer.arg("wifiPass") != String("********"))
    {
      webServer.arg("wifiPass").toCharArray(config.getWIFIPass(), 64);
    }
  }
  if (webServer.arg("mqttServer") != "" && webServer.arg("mqttServer") != String(config.getMQTTServer()))
  { // Handle mqttServer
    config.setSaveNeeded();
    webServer.arg("mqttServer").toCharArray(config.getMQTTServer(), 64);
  }
  if (webServer.arg("mqttPort") != "" && webServer.arg("mqttPort") != String(config.getMQTTPort()))
  { // Handle mqttPort
    config.setSaveNeeded();
    webServer.arg("mqttPort").toCharArray(config.getMQTTPort(), 6);
  }
  if (webServer.arg("haspNode") != "" && webServer.arg("haspNode") != String(config.getHaspNode()))
  { // Handle haspNode
    config.setSaveNeeded();
    String lowerHaspNode = webServer.arg("haspNode");
    lowerHaspNode.toLowerCase();
    lowerHaspNode.toCharArray(config.getHaspNode(), 16);
  }
  if (webServer.arg("groupName") != "" && webServer.arg("groupName") != String(config.getGroupName()))
  { // Handle groupName
    config.setSaveNeeded();
    webServer.arg("groupName").toCharArray(config.getGroupName(), 16);
  }
  // Check optional values
  if (webServer.arg("mqttUser") != String(config.getMQTTUser()))
  { // Handle mqttUser
    config.setSaveNeeded();
    webServer.arg("mqttUser").toCharArray(config.getMQTTUser(), 32);
  }
  if (webServer.arg("mqttPassword") != String("********"))
  { // Handle mqttPassword
    config.setSaveNeeded();
    webServer.arg("mqttPassword").toCharArray(config.getMQTTPassword(), 32);
  }
  if (webServer.arg("configUser") != String(_configUser))
  { // Handle configUser
    config.setSaveNeeded();
    webServer.arg("configUser").toCharArray(_configUser, 32);
  }
  if (webServer.arg("configPassword") != String("********"))
  { // Handle configPassword
    config.setSaveNeeded();
    webServer.arg("configPassword").toCharArray(_configPassword, 32);
  }
  if (webServer.arg("motionPinConfig") != String(config.getMotionPin()))
  { // Handle motionPinConfig
    config.setSaveNeeded();
    webServer.arg("motionPinConfig").toCharArray(config.getMotionPin(), 3);
  }
  if ((webServer.arg("debugSerialEnabled") == String("on")) && !debug.getSerialEnabled())
  { // debugSerialEnabled was disabled but should now be enabled
    config.setSaveNeeded();
    debug.enableSerial(true);
  }
  else if ((webServer.arg("debugSerialEnabled") == String("")) && debug.getSerialEnabled())
  { // debugSerialEnabled was enabled but should now be disabled
    config.setSaveNeeded();
    debug.enableSerial(false);
  }
  if ((webServer.arg("debugTelnetEnabled") == String("on")) && !debug.getTelnetEnabled())
  { // debugTelnetEnabled was disabled but should now be enabled
    config.setSaveNeeded();
    debug.enableTelnet(true);
  }
  else if ((webServer.arg("debugTelnetEnabled") == String("")) && debug.getTelnetEnabled())
  { // debugTelnetEnabled was enabled but should now be disabled
    config.setSaveNeeded();
    debug.enableTelnet(false);
  }
  if ((webServer.arg("mdnsEnabled") == String("on")) && !config.getMDNSEnabled())
  { // mdnsEnabled was disabled but should now be enabled
    config.setSaveNeeded();
    config.setMDSNEnabled(true);
  }
  else if ((webServer.arg("mdnsEnabled") == String("")) && config.getMDNSEnabled())
  { // mdnsEnabled was enabled but should now be disabled
    config.setSaveNeeded();
    config.setMDSNEnabled(false);
  }
  if ((webServer.arg("beepEnabled") == String("on")) && !beep.getEnable())
  { // beepEnabled was disabled but should now be enabled
    config.setSaveNeeded();
    beep.enable(true);
  }
  else if ((webServer.arg("beepEnabled") == String("")) && beep.getEnable())
  { // beepEnabled was enabled but should now be disabled
    config.setSaveNeeded();
    beep.enable(true);
  }

  if (config.getSaveNeeded())
  { // Config updated, notify user and trigger write to SPIFFS
    httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
    httpMessage += FPSTR(HTTP_HEADER_END);
    httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + String(F("</h1>"));
    httpMessage += String(F("<br/>Saving updated configuration values and restarting device"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);

    config.saveFile();
    if (shouldSaveWifi)
    {
      debug.printLn(String(F("CONFIG: Attempting connection to SSID: ")) + webServer.arg("wifiSSID"));
      esp.wiFiSetup();
    }
    esp.reset();
  }
  else
  { // No change found, notify user and link back to config page
    httpMessage += String(F("<meta http-equiv='refresh' content='3;url=/' />"));
    httpMessage += FPSTR(HTTP_HEADER_END);
    httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + String(F("</h1>"));
    httpMessage += String(F("<br/>No changes found, returning to <a href='/'>home page</a>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleResetConfig()
{ // http://plate01/resetConfig
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /resetConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", String(config.getHaspNode()));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += FPSTR(HTTP_HEADER_END);

  if (webServer.arg("confirm") == "yes")
  { // User has confirmed, so reset everything
    httpMessage += String(F("<h1>"));
    httpMessage += String(config.getHaspNode());
    httpMessage += String(F("</h1><b>Resetting all saved settings and restarting device into WiFi AP mode</b>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
    delay(1000);
    config.clearFileSystem();
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
void WebClass::_handleResetBacklight()
{ // http://plate01/resetBacklight
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /resetBacklight page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " HASP backlight reset"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += String(F("<meta http-equiv='refresh' content='3;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + String(F("</h1>"));
  httpMessage += String(F("<br/>Resetting backlight to 100%"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  debug.printLn(F("HTTP: Resetting backlight to 100%"));
  nextion.setAttr("dims", "100");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleFirmware()
{ // http://plate01/firmware
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /firmware page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + String(F(" firmware</h1>"));

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
void WebClass::_handleEspFirmware()
{ // http://plate01/espfirmware
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /espfirmware page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " ESP update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += String(F("<meta http-equiv='refresh' content='60;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(config.getHaspNode()) + " ESP update";
  httpMessage += String(F("</h1>"));
  httpMessage += "<br/>Updating ESP firmware from: " + String(webServer.arg("espFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);

  debug.printLn("ESPFW: Attempting ESP firmware update from: " + String(webServer.arg("espFirmware")));
  esp.startOta(webServer.arg("espFirmware"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleLcdUpload()
{ // http://plate01/lcdupload
  // Upload firmware to the Nextion LCD via HTTP upload

  if( !_authenticated() ) { return; }

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
    httpMessage.replace("{v}", (String(config.getHaspNode()) + " LCD update"));
    httpMessage += FPSTR(HTTP_SCRIPT);
    httpMessage += FPSTR(HTTP_STYLE);
    httpMessage += String(_haspStyle);
    httpMessage += String(F("<meta http-equiv='refresh' content='5;url=/' />"));
    httpMessage += FPSTR(HTTP_HEADER_END);
    httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + " LCD update FAILED</h1>";
    httpMessage += String(F("No update file size reported.  You must use a modern browser with Javascript enabled."));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
  else if ((lcdOtaTimer > 0) && ((millis() - lcdOtaTimer) > lcdOtaTimeout))
  { // Our timer expired so reset
    debug.printLn(F("LCD OTA: ERROR: LCD upload timeout.  Restarting."));
    esp.reset();
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
    nextion.handleInput();

    String lcdOtaNextionCmd = "whmi-wri " + String(tftFileSize) + ",115200,0";
    debug.printLn(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
    Serial1.print(lcdOtaNextionCmd);
    Serial1.write(nextion.Suffix, sizeof(nextion.Suffix));
    Serial1.flush();

    if (nextion.otaResponse())
    {
      debug.printLn(F("LCD OTA: LCD upload command accepted"));
    }
    else
    {
      debug.printLn(F("LCD OTA: LCD upload command FAILED."));
      esp.reset();
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
        if (nextion.otaResponse())
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
      if (nextion.otaResponse())
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
        esp.reset();
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
        esp.reset();
      }
    }
    lcdOtaTimer = millis();
  }
  else if (upload.status == UPLOAD_FILE_END)
  { // Upload completed
    if (lcdOtaTransferred >= tftFileSize)
    {
      if (nextion.otaResponse())
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
        esp.reset();
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
        esp.reset();
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
    esp.reset();
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
    esp.reset();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleLcdUpdateSuccess()
{ // http://plate01/lcdOtaSuccess
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /lcdOtaSuccess page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " LCD update success"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + String(F(" LCD update success</h1>"));
  httpMessage += String(F("Restarting HASwitchPlate to apply changes..."));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleLcdUpdateFailure()
{ // http://plate01/lcdOtaFailure
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /lcdOtaFailure page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " LCD update failed"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + String(F(" LCD update failed :(</h1>"));
  httpMessage += String(F("Restarting HASwitchPlate to reset device..."));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleLcdDownload()
{ // http://plate01/lcddownload
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /lcddownload page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " LCD update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(config.getHaspNode()) + " LCD update";
  httpMessage += String(F("</h1>"));
  httpMessage += "<br/>Updating LCD firmware from: " + String(webServer.arg("lcdFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);

  nextion.startOtaDownload(webServer.arg("lcdFirmware"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleTftFileSize()
{ // http://plate01/tftFileSize
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /tftFileSize page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " TFT Filesize"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  tftFileSize = webServer.arg("tftFileSize").toInt();
  debug.printLn(String(F("WEB: tftFileSize: ")) + String(tftFileSize));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WebClass::_handleReboot()
{ // http://plate01/reboot
  if( !_authenticated() ) { return; }

  debug.printLn(String(F("HTTP: Sending /reboot page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEADER);
  httpMessage.replace("{v}", (String(config.getHaspNode()) + " HASP reboot"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(_haspStyle);
  httpMessage += String(F("<meta http-equiv='refresh' content='10;url=/' />"));
  httpMessage += FPSTR(HTTP_HEADER_END);
  httpMessage += String(F("<h1>")) + String(config.getHaspNode()) + String(F("</h1>"));
  httpMessage += String(F("<br/>Rebooting device"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  debug.printLn(F("RESET: Rebooting device"));
  nextion.sendCmd("page 0");
  nextion.setAttr("p[0].b[1].txt", "\"Rebooting...\"");
  esp.reset();
}
