// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// hmi_nextion.cpp : Class internals to interact with the Nextion-brand HMI LCD touchscreen
//
// ----------------------------------------------------------------------------------------------------------------- //

#include "hmi_nextion.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>

#include "debug.h"
extern debugClass debug; // our debug Object, USB Serial and/or Telnet

#include "config_class.h"
extern ConfigClass config; // our Configuration Container

#include "mqtt_class.h"
extern MQTTClass mqtt;   // our MQTT Object

#include "web_class.h"
extern WebClass web; // our HTTP Object

#include "speaker_class.h"
extern SpeakerClass beep; // our Speaker Object


////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Class These!
extern String getSubtringField(String data, char separator, int index); // TODO: class me
extern String printHex8(String data, uint8_t length); // TODO: class me
extern bool updateCheck();
extern void espReset();

// Is this ours or webClass'?
uint32_t tftFileSize = 0;                           // Filesize for TFT firmware upload

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::begin(void)
{  // called in the main code setup, handles our initialisation
  _alive               = true;
  _startupCompleteFlag = false;
  _checkTimer          = 0;
  _retryMax            = NEXTION_RETRY_MAX;
  _lcdConnected        = false;
  _lcdVersionQueryFlag = false;
  _lcdVersion          = 0;
  _returnIndex         = 0;
  _activePage          = 0;
  _reportPage0         = NEXTION_REPORT_PAGE0;

#if NEXTION_CACHE_ENABLED==(true)
  // setting the cache up goes here too
  for( int idx=0; idx<_cachePageCount; idx++)
  {
    _pageCache[idx]=NULL; // null terminate each cache at power-on
    _pageCacheLen[idx]=0;
    _pageIsGlobal[idx]=false; // default to local scope
  }
  // any pages that default to GlobalScope could go here too
#endif // NEXTION_CACHE_ENABLED

  while (!_lcdConnected && (millis() < 5000))
  { // Wait up to 5 seconds for serial input from LCD
    handleInput();
  }
  if (_lcdConnected)
  {
    debug.printLn(F("HMI: LCD responding, continuing program load"));
    sendCmd("connect");
  }
  else
  {
    debug.printLn(F("HMI: LCD not responding, continuing program load"));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::loop(void)
{ // called in the main code loop, handles our periodic code
  if( !_alive )
  {
    begin();
  }
  if (handleInput())
  { // Process user input from HMI
    processInput();
  }

  if ((_lcdVersion < 1) && (millis() <= (_retryMax * CheckInterval)))
  { // Attempt to connect to LCD panel to collect model and version info during startup
    _connect();
  }
  else if ((_lcdVersion > 0) && (millis() <= (_retryMax * CheckInterval)) && !_startupCompleteFlag)
  { // We have LCD info, so trigger an update check + report
    if (updateCheck())
    { // Send a status update if the update check worked
      mqtt.statusUpdate();
      _startupCompleteFlag = true;
    }
  }
  else if ((millis() > (_retryMax * CheckInterval)) && !_startupCompleteFlag)
  { // We still don't have LCD info so go ahead and run the rest of the checks once at startup anyway
    updateCheck();
    mqtt.statusUpdate();
    _startupCompleteFlag = true;
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::reset()
{ // reset the LCD (often by denying power to the display)
  debug.printLn(F("HMI: Rebooting LCD"));
  digitalWrite(_resetPin, LOW);
  Serial1.print("rest"); // yes "rest", not "reset"
  Serial1.write(Suffix, sizeof(Suffix));
  Serial1.flush();
  delay(100);
  digitalWrite(_resetPin, HIGH);

  uint32_t lcdResetTimer = millis();
  const uint32_t lcdResetTimeout = 5000;

  _lcdConnected = false;
  while (!_lcdConnected && (millis() < (lcdResetTimer + lcdResetTimeout)))
  {
    handleInput();
  }
  if (_lcdConnected)
  {
    debug.printLn(F("HMI: Rebooting LCD completed"));
    if (_activePage)
    {
      sendCmd("page " + String(_activePage));
    }
  }
  else
  {
    debug.printLn(F("ERROR: Rebooting LCD completed, but LCD is not responding."));
  }
  mqtt.publishStatusTopic("OFF");

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::initResetPin()
{ // Initialise the Reset Pin and set it High so the LCD gets power
  _resetPin = NEXTION_RESET_PIN;
  pinMode(_resetPin, OUTPUT);
  digitalWrite(_resetPin, HIGH);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::getAttr(String hmiAttribute)
{ // Get the value of a Nextion component attribute
  // This will only send the command to the panel requesting the attribute, the actual
  // return of that value will be handled by processInput and placed into mqttGetSubtopic
  Serial1.print("get " + hmiAttribute);
  Serial1.write(Suffix, sizeof(Suffix));
  debug.printLn(HMI,String(F("HMI OUT: 'get ")) + hmiAttribute + "'");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::sendCmd(String cmd)
{
#if NEXTION_CACHE_ENABLED!=(true)
  if( !useCache )
  { // No cache, just send all commands straight to the panel
    _sendCmd(cmd);
    return;
  }
#else
  // Yes cache, only send some commands to the panel

  // p[1].b[1].text="Hello World"
  if( cmd.startsWith("p[") && (cmd.charAt(3)==']' || cmd.charAt(4)==']'))
  {
    int tgtPage;
    // who wants to bet there is a cleaner way to turn p[0] to p[99] into integer 0 to 99?
    if(cmd.charAt(3)==']')
    {
      tgtPage=(cmd.charAt(2)-'0');
    }
    else
    {
      tgtPage=((cmd.charAt(2)-'0')*10) + (cmd.charAt(3)-'0');
    }

    // Always add the entry to the list
    _appendCmd(tgtPage,cmd);

    // but only render it if the page is active, or the page is global
    if (tgtPage == _activePage || _pageIsGlobal[tgtPage])
    {
      _sendCmd(cmd);
    }
    // else
    // {
    // debug.printLn(HMI,String(F("HMI Skip: ")) + cmd);
    // }

  }
  else
  { // not a button / attribute, send it to the panel
    _sendCmd(cmd);
  }
#endif // NEXTION_CACHE_ENABLED
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::parseJson(String &strPayload)
{ // Parse an incoming JSON array into individual Nextion commands
  if (strPayload.endsWith(",]"))
  { // Trailing null array elements are an artifact of older Home Assistant automations and need to
    // be removed before parsing by ArduinoJSON 6+
    strPayload.remove(strPayload.length() - 2, 2);
    strPayload.concat("]");
  }
  DynamicJsonDocument commands(mqttMaxPacketSize + 1024);
  DeserializationError jsonError = deserializeJson(commands, strPayload);
  if (jsonError)
  { // Couldn't parse incoming JSON command
    debug.printLn(HMI,String(F("MQTT: [ERROR] Failed to parse incoming JSON command with error: ")) + String(jsonError.c_str()));
  }
  else
  {
    for (uint8_t i = 0; i < commands.size(); i++)
    {
      sendCmd(commands[i]);
      delayMicroseconds(500); // Larger JSON objects can take a while to run through over serial,
    }                         // give the ESP and Nextion a moment to deal with life
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::handleInput()
{ // Handle incoming serial data from the Nextion panel
  // This will collect serial data from the panel and place it into the global buffer
  // _returnBuffer[_returnIndex]
  // Return: true if we've received a string of 3 consecutive 0xFF values
  // Return: false otherwise
  bool commandComplete = false;
  static int termByteCnt = 0;   // counter for our 3 consecutive 0xFFs
  static String hmiDebugMsg = "HMI IN: "; // assemble a string for debug output

  if (Serial.available())
  {
    _lcdConnected = true;
    uint8_t commandByte = Serial.read();
    hmiDebugMsg += (" 0x" + String(commandByte, HEX));
    // check to see if we have one of 3 consecutive 0xFF which indicates the end of a command
    if (commandByte == 0xFF)
    {
      termByteCnt++;
      if (termByteCnt >= 3)
      { // We have received a complete command
        commandComplete = true;
        termByteCnt = 0; // reset counter
      }
    }
    else
    {
      termByteCnt = 0; // reset counter if a non-term byte was encountered
    }
    _returnBuffer[_returnIndex] = commandByte;
    _returnIndex++;
  }
  if (commandComplete)
  {
    debug.printLn(HMI, hmiDebugMsg);
    hmiDebugMsg = "HMI IN: ";
  }
  return commandComplete;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::processInput()
{ // Process incoming serial commands from the Nextion panel
  // Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
  // tl;dr: command uint8_t, command data, 0xFF 0xFF 0xFF

  if (_returnBuffer[0] == 0x65)
  { // Handle incoming touch command
    // 0x65+Page ID+Component ID+TouchEvent+End
    // Return this data when the touch event created by the user is pressed.
    // Definition of TouchEvent: Press Event 0x01, Release Event 0X00
    // Example: 0x65 0x00 0x02 0x01 0xFF 0xFF 0xFF
    // Meaning: Touch Event, Page 0, Object 2, Press
    String page = String(_returnBuffer[1]);
    String buttonID = String(_returnBuffer[2]);
    uint8_t buttonAction = _returnBuffer[3];

    if (buttonAction == 0x01)
    {
      debug.printLn(HMI, String(F("HMI IN: [Button ON] 'p[")) + page + "].b[" + buttonID + "]'");

      mqtt.publishButtonEvent(page, buttonID, "ON");
      beep.playSound(500,100,1);
    }
    if (buttonAction == 0x00)
    {
      debug.printLn(HMI, String(F("HMI IN: [Button OFF] 'p[")) + page + "].b[" + buttonID + "]'");
      mqtt.publishButtonEvent(page, buttonID, "OFF");

      // Now see if this object has a .val that might have been updated.  Works for sliders,
      // two-state buttons, etc, throws a 0x1A error for normal buttons which we'll catch and ignore
      _mqttGetSubtopic = "/p[" + page + "].b[" + buttonID + "].val";
      _mqttGetSubtopicJSON = "p[" + page + "].b[" + buttonID + "].val";
      getAttr("p[" + page + "].b[" + buttonID + "].val");
    }
  }
  else if (_returnBuffer[0] == 0x66)
  { // Handle incoming "sendme" page number
    // 0x66+PageNum+End
    // Example: 0x66 0x02 0xFF 0xFF 0xFF
    // Meaning: page 2
    String page = String(_returnBuffer[1]);
    debug.printLn(HMI, String(F("HMI IN: [sendme Page] '")) + page + "'");
    // if ((_activePage != page.toInt()) && ((page != "0") || _reportPage0))
    if ((page != "0") || _reportPage0)
    { // If we have a new page AND ( (it's not "0") OR (we've set the flag to report 0 anyway) )
      _activePage = page.toInt();
      _replayCmd();
      mqtt.publishStatePage(page);
    }
  }
  else if (_returnBuffer[0] == 0x67)
  { // Handle touch coordinate data
    // 0X67+Coordinate X High+Coordinate X Low+Coordinate Y High+Coordinate Y Low+TouchEvent+End
    // Example: 0X67 0X00 0X7A 0X00 0X1E 0X01 0XFF 0XFF 0XFF
    // Meaning: Coordinate (122,30), Touch Event: Press
    // issue  command "sendxy=1" to enable this output
    uint16_t xCoord = _returnBuffer[1];
    xCoord = (xCoord<<8) | _returnBuffer[2];
    uint16_t yCoord = _returnBuffer[3];
    yCoord = (yCoord<<8) | _returnBuffer[4];
    String xyCoord = String(xCoord) + ',' + String(yCoord);
    uint8_t TouchAction = _returnBuffer[5];
    if (TouchAction == 0x01)
    {
      debug.printLn(HMI,String(F("HMI IN: [Touch ON] '")) + xyCoord + "'");
      mqtt.publishStateSubTopic(String(F("/touchOn")), xyCoord);
    }
    else if (TouchAction == 0x00)
    {
      debug.printLn(HMI,String(F("HMI IN: [Touch OFF] '")) + xyCoord + "'");
      mqtt.publishStateSubTopic(String(F("/touchOff")), xyCoord);
    }
  }
  else if (_returnBuffer[0] == 0x70)
  { // Handle get string return
    // 0x70+ASCII string+End
    // Example: 0x70 0x41 0x42 0x43 0x44 0x31 0x32 0x33 0x34 0xFF 0xFF 0xFF
    // Meaning: String data, ABCD1234
    String getString;
    for (int i = 1; i < _returnIndex - 3; i++)
    { // convert the payload into a string
      getString += (char)_returnBuffer[i];
    }
    debug.printLn(HMI,String(F("HMI IN: [String Return] '")) + getString + "'");
    if (_mqttGetSubtopic == "")
    { // If there's no outstanding request for a value, publish to mqttStateTopic
      mqtt.publishStateTopic(getString);
    }
    else
    { // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
      mqtt.publishStateSubTopic(_mqttGetSubtopic, getString);
      _mqttGetSubtopic = "";
    }
  }
  else if (_returnBuffer[0] == 0x71)
  { // Handle get int return
    // 0x71+byte1+byte2+byte3+byte4+End (4 byte little endian)
    // Example: 0x71 0x7B 0x00 0x00 0x00 0xFF 0xFF 0xFF
    // Meaning: Integer data, 123
    uint32_t getInt = _returnBuffer[4];
    getInt = (getInt << 8) | _returnBuffer[3];
    getInt = (getInt << 8) | _returnBuffer[2];
    getInt = (getInt << 8) | _returnBuffer[1];
    String getString = String(getInt);
    debug.printLn(HMI,String(F("HMI IN: [Int Return] '")) + getString + "'");

    if (_lcdVersionQueryFlag)
    {
      _lcdVersion = getInt;
      _lcdVersionQueryFlag = false;
      debug.printLn(HMI,String(F("HMI IN: lcdVersion '")) + String(_lcdVersion) + "'");
    }
    else if (_mqttGetSubtopic == "")
    {
      mqtt.publishStateTopic(getString);
    }
    // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
    else
    {
      mqtt.publishStateSubTopic(_mqttGetSubtopic, getString);
      _mqttGetSubtopic = "";
    }
  }
  else if (_returnBuffer[0] == 0x63 && _returnBuffer[1] == 0x6f && _returnBuffer[2] == 0x6d && _returnBuffer[3] == 0x6f && _returnBuffer[4] == 0x6b)
  { // Catch 'comok' response to 'connect' command: https://www.itead.cc/blog/nextion-hmi-upload-protocol
    String comokField;
    uint8_t comokFieldCount = 0;
    uint8_t comokFieldSeperator = 0x2c; // ","

    for (uint8_t i = 0; i <= _returnIndex; i++)
    { // cycle through each byte looking for our field seperator
      if (_returnBuffer[i] == comokFieldSeperator)
      { // Found the end of a field, so do something with it.  Maybe.
        if (comokFieldCount == 2)
        {
          _model = comokField;
          debug.printLn(HMI,String(F("HMI IN: NextionModel: ")) + _model);
        }
        comokFieldCount++;
        comokField = "";
      }
      else
      {
        comokField += String(char(_returnBuffer[i]));
      }
    }
  }
  else if (_returnBuffer[0] == 0x1A)
  { // Catch 0x1A error, possibly from .val query against things that might not support that request
    // 0x1A+End
    // ERROR: Variable name invalid
    // We'll be triggering this a lot due to requesting .val on every component that sends us a Touch Off
    // Just reset mqttGetSubtopic and move on with life.
    _mqttGetSubtopic = "";
  }
  _returnIndex = 0; // Done handling the buffer, reset index back to 0
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::startOtaDownload(String otaUrl)
{ // Upload firmware to the Nextion LCD via HTTP download
  // based in large part on code posted by indev2 here:
  // http://support.iteadstudio.com/support/discussions/topics/11000007686/page/2

  uint32_t lcdOtaFileSize = 0;
  String lcdOtaNextionCmd;
  uint32_t lcdOtaChunkCounter = 0;
  uint16_t lcdOtaPartNum = 0;
  uint32_t lcdOtaTransferred = 0;
  uint8_t lcdOtaPercentComplete = 0;
  const uint32_t lcdOtaTimeout = 30000; // timeout for receiving new data in milliseconds
  static uint32_t lcdOtaTimer = 0;      // timer for upload timeout

  debug.printLn(String(F("LCD OTA: Attempting firmware download from: ")) + otaUrl);
  WiFiClient lcdOtaWifi;
  HTTPClient lcdOtaHttp;
  lcdOtaHttp.begin(lcdOtaWifi, otaUrl);
  int lcdOtaHttpReturn = lcdOtaHttp.GET();
  if (lcdOtaHttpReturn > 0)
  { // HTTP header has been sent and Server response header has been handled
    debug.printLn(String(F("LCD OTA: HTTP GET return code:")) + String(lcdOtaHttpReturn));
    if (lcdOtaHttpReturn == HTTP_CODE_OK)
    {                                                 // file found at server
      int32_t lcdOtaRemaining = lcdOtaHttp.getSize(); // get length of document (is -1 when Server sends no Content-Length header)
      lcdOtaFileSize = lcdOtaRemaining;
      static uint16_t lcdOtaParts = (lcdOtaRemaining / 4096) + 1;
      static const uint16_t lcdOtaBufferSize = 1024; // upload data buffer before sending to UART
      static uint8_t lcdOtaBuffer[lcdOtaBufferSize] = {};

      debug.printLn(String(F("LCD OTA: File found at Server. Size ")) + String(lcdOtaRemaining) + String(F(" bytes in ")) + String(lcdOtaParts) + String(F(" 4k chunks.")));

      WiFiUDP::stopAll(); // Keep mDNS responder and MQTT traffic from breaking things
      if (mqtt.clientIsConnected())
      {
        debug.printLn(F("LCD OTA: LCD firmware upload starting, closing MQTT connection."));
        mqtt.goodbye();
      }

      WiFiClient *stream = lcdOtaHttp.getStreamPtr();      // get tcp stream
      Serial1.write(Suffix, sizeof(Suffix)); // Send empty command
      Serial1.flush();
      handleInput();
      String lcdOtaNextionCmd = "whmi-wri " + String(lcdOtaFileSize) + ",115200,0";
      debug.printLn(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
      Serial1.print(lcdOtaNextionCmd);
      Serial1.write(Suffix, sizeof(Suffix));
      Serial1.flush();

      if (otaResponse())
      {
        debug.printLn(F("LCD OTA: LCD upload command accepted."));
      }
      else
      {
        debug.printLn(F("LCD OTA: LCD upload command FAILED.  Restarting device."));
        espReset();
      }
      debug.printLn(F("LCD OTA: Starting update"));
      lcdOtaTimer = millis();
      while (lcdOtaHttp.connected() && (lcdOtaRemaining > 0 || lcdOtaRemaining == -1))
      {                                                // Write incoming data to panel as it arrives
        uint16_t lcdOtaHttpSize = stream->available(); // get available data size

        if (lcdOtaHttpSize)
        {
          uint16_t lcdOtaChunkSize = 0;
          if ((lcdOtaHttpSize <= lcdOtaBufferSize) && (lcdOtaHttpSize <= (4096 - lcdOtaChunkCounter)))
          {
            lcdOtaChunkSize = lcdOtaHttpSize;
          }
          else if ((lcdOtaBufferSize <= lcdOtaHttpSize) && (lcdOtaBufferSize <= (4096 - lcdOtaChunkCounter)))
          {
            lcdOtaChunkSize = lcdOtaBufferSize;
          }
          else
          {
            lcdOtaChunkSize = 4096 - lcdOtaChunkCounter;
          }
          stream->readBytes(lcdOtaBuffer, lcdOtaChunkSize);
          Serial1.flush();                              // make sure any previous writes the UART have completed
          Serial1.write(lcdOtaBuffer, lcdOtaChunkSize); // now send buffer to the UART
          lcdOtaChunkCounter += lcdOtaChunkSize;
          if (lcdOtaChunkCounter >= 4096)
          {
            Serial1.flush();
            lcdOtaPartNum++;
            lcdOtaTransferred += lcdOtaChunkCounter;
            lcdOtaPercentComplete = (lcdOtaTransferred * 100) / lcdOtaFileSize;
            lcdOtaChunkCounter = 0;
            if (otaResponse())
            { // We've completed a chunk
              debug.printLn(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" OK, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
              lcdOtaTimer = millis();
            }
            else
            {
              debug.printLn(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" FAILED, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
              debug.printLn(F("LCD OTA: failure"));
              delay(2000); // extra delay while the LCD does its thing
              espReset();
            }
          }
          else
          {
            delay(20);
          }
          if (lcdOtaRemaining > 0)
          {
            lcdOtaRemaining -= lcdOtaChunkSize;
          }
        }
        delay(10);
        if ((lcdOtaTimer > 0) && ((millis() - lcdOtaTimer) > lcdOtaTimeout))
        { // Our timer expired so reset
          debug.printLn(F("LCD OTA: ERROR: LCD upload timeout.  Restarting."));
          espReset();
        }
      }
      lcdOtaPartNum++;
      lcdOtaTransferred += lcdOtaChunkCounter;
      if ((lcdOtaTransferred == lcdOtaFileSize) && otaResponse())
      {
        debug.printLn(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 5000)
        { // extra 5sec delay while the LCD handles any local firmware updates from new versions of code sent to it
          web.loop();
          delay(1);
        }
        espReset();
      }
      else
      {
        debug.printLn(F("LCD OTA: Failure"));
        espReset();
      }
    }
  }
  else
  {
    debug.printLn(String(F("LCD OTA: HTTP GET failed, error code ")) + lcdOtaHttp.errorToString(lcdOtaHttpReturn));
    espReset();
  }
  lcdOtaHttp.end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::otaResponse()
{ // Monitor the serial port for a 0x05 response within our timeout

  uint32_t nextionCommandTimeout = 2000;   // timeout for receiving termination string in milliseconds
  uint32_t nextionCommandTimer = millis(); // record current time for our timeout
  bool otaSuccessVal = false;
  while ((millis() - nextionCommandTimer) < nextionCommandTimeout)
  {
    if (Serial.available())
    {
      uint8_t inByte = Serial.read();
      if (inByte == 0x5)
      {
        otaSuccessVal = true;
        break;
      }
    }
    else
    {
      delay(1); // delay harms esp8266 wifi
    }
  }
  return otaSuccessVal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::debug_page_cache(void)
{
  if( !useCache ) { return; }
#if NEXTION_CACHE_ENABLED==(true)
  debug.printLn(String(F("")));
  for( int idx=0;idx<_cachePageCount;idx++) {
    debug.printLn(String(F("debug [")) + idx + String(F("]=")) + _pageCacheLen[idx] );
  }
  debug.printLn(String(F("")));
#endif // NEXTION_CACHE_ENABLED
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::setActivePage(uint8_t newPage )
{
  // bounds checks? dependent effects?
  _activePage=newPage;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::changePage(uint8_t page) {
  if (getActivePage() != page)
  { // Hass likes to send duplicate responses to things like page requests and there are no plans to fix that behavior, so try and track it locally
    _activePage=page;
    sendCmd("page " + String(_activePage));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::setPageGlobal( uint8_t page, bool newFlag )
{
#if NEXTION_CACHE_ENABLED==(true)
  if( page < _cachePageCount )
  {
    _pageIsGlobal[page] = newFlag;
  }
  else
  {
    debug.printLn(HMI,String(F("Cache cannot be global for high-order page: ")) + page );
  }
#endif // NEXTION_CACHE_ENABLED
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::_sendCmd(String cmd)
{ // Send a raw command to the Nextion panel
  Serial1.print(cmd);
  Serial1.write(Suffix, sizeof(Suffix));
  debug.printLn(HMI,String(F("HMI OUT: ")) + cmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::_connect()
{ // connect to the Nextion Panel
  if ((millis() - _checkTimer) >= CheckInterval)
  {
    static uint32_t retryCount = 0;
    if ((_model.length() == 0) && (retryCount < (_retryMax - 2)))
    { // Try issuing the "connect" command a few times
      debug.printLn(HMI, F("HMI: sending Nextion connect request"));
      sendCmd("connect");
      retryCount++;
      _checkTimer = millis();
    }
    else if ((_model.length() == 0) && (retryCount < _retryMax))
    { // If we still don't have model info, try to change nextion serial speed from 9600 to 115200
      _setSpeed();
      retryCount++;
      debug.printLn(HMI, F("HMI: sending Nextion serial speed 115200 request"));
      _checkTimer = millis();
    }
    else if ((_lcdVersion < 1) && (retryCount <= _retryMax))
    {
      if (_model.length() == 0)
      { // one last hail mary, maybe the serial speed is set correctly now
        sendCmd("connect");
      }
      sendCmd("get " + _lcdVersionQuery);
      _lcdVersionQueryFlag = true;
      retryCount++;
      debug.printLn(HMI, F("HMI: sending Nextion version query"));
      _checkTimer = millis();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::_setSpeed()
{ // Set the Nextion serial port speed
  debug.printLn(HMI,F("HMI: No Nextion response, attempting 9600bps connection"));
  Serial1.begin(9600);
  Serial1.write(Suffix, sizeof(Suffix));
  Serial1.print("bauds=115200");
  Serial1.write(Suffix, sizeof(Suffix));
  Serial1.flush();
  Serial1.begin(115200);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::_replayCmd(void)
{ // play entries from the cache to the panel
  if( !useCache ) { return; }
#if NEXTION_CACHE_ENABLED==(true)
  //  debug.printLn(HMI,String(F("--- Entry Point Replay Cmd ")) + _activePage);
  //  if( 1 == _activePage)
  //  {
  //    debug.printLn(HMI,String(F("--- Contents  ")) + _pageCache[_activePage]);
  //  }
  if( _activePage >= _cachePageCount )
  {
    debug.printLn(HMI, String(F("Cache cannot replay for high-order page: ")) + _activePage );
    return;
  }
  // Q: how badly do all these strings composed this way chew our free RAM?
  String preface=String(F("p[")) + _activePage + String(F("]."));
  uint32_t bitIdx=1;
  for( uint8_t idx=0; idx<_cacheButtonCount; idx++ )
  {
    // we need these a few times, so save them here
    button_t *current = &(_cached[_activePage][idx]);
    have_t *have = &(_cache_has[_activePage]);
    String midface=String(F("b[")) + idx + String(F("]."));
    if (have->font & bitIdx)
    {
      String resultant = preface + midface + String(F("font=")) + current->font;
      _sendCmd(resultant);
    }
    if (have->pco & bitIdx)
    {
      String resultant = preface + midface + String(F("pco=")) + current->pco;
      _sendCmd(resultant);
    }
    if (have->bco & bitIdx)
    {
      String resultant = preface + midface + String(F("bco=")) + current->bco;
      _sendCmd(resultant);
    }
    if (have->pco2 & bitIdx)
    {
      String resultant = preface + midface + String(F("pco2=")) + current->pco2;
      _sendCmd(resultant);
    }
    if (have->bco2 & bitIdx)
    {
      String resultant = preface + midface + String(F("bco2=")) + current->bco2;
      _sendCmd(resultant);
    }
    if (have->xcen & bitIdx)
    {
      String resultant = preface + midface + String(F("xcen=")) + current->xcen;
      _sendCmd(resultant);
    }
    // gate twice to reduce NULL dereferences
    if (have->txt & bitIdx && current->txtlen > 0 )
    {
      String resultant = preface + midface + String(F("txt=")) + current->txt;
      _sendCmd(resultant);
    }
    // we could count writes and delay here if we are overloading the LCD/Serial port

    // increment our boolean selector
    bitIdx <<= 1;
  }

  // Legacy Cache using JSON that sometimes fails:
  // Do nothing if the cache is empty
  if( _pageCache[_activePage] == NULL )
  {
    return;
  }
  // Do something if the cache is not.
  //StaticJsonDocument<_cacheBufferSize> replayCommands;
  DynamicJsonDocument replayCommands(_cacheBufferSize);
  DeserializationError jsonError = deserializeJson(replayCommands, _pageCache[_activePage]);
  if (jsonError)
  { // Couldn't parse incoming JSON command
    debug.printLn(HMI,String(F("HMI: [ERROR] Failed to replay cache on page change. Reported error: ")) + String(jsonError.c_str()));
  }
  else
  {
    // we saved ram by not storing the p[x]. text, so re-add it here
    // can we do this with printf?
    JsonObject replayObj = replayCommands.as<JsonObject>();
    //JsonArray replayArray = replayCommands.as<JsonArray>();
    //    if( 1 == _activePage )
    //    {
    //      debug.printLn(HMI,String(F("--- ah wea  ")) + replayCommands.size() + "  " + replayObj.size() + "  " + preface);
    //    }
    for (JsonPair keyValue : replayObj)
    {
      //String subsequent = replayCommands[idx];
      String thiskey=keyValue.key().c_str();
      String thisvalue=keyValue.value().as<char*>();
      String resultant = preface + thiskey + "=" + thisvalue;
      //      debug.printLn(HMI,String(F("HMI:  ")) + " " + resultant); // _sendCmd has a printf itself
      _sendCmd(resultant);
      delayMicroseconds(200); // Larger JSON objects can take a while to run through over serial,
    }                         // give the ESP and Nextion a moment to deal with life
  }
#endif // NEXTION_CACHE_ENABLED
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void hmiNextionClass::_appendCmd(int page, String cmd)
{ // add entries to the cache, and pass input that is unsupported or for the activePage to the panel too
  if( !useCache ) { return; }
#if NEXTION_CACHE_ENABLED==(true)
  //  debug.printLn(HMI,String(F("aCmd ")) + page + "  " + cmd);
  // our input is like p[1].b[1].txt="Hello World"
  // or p[20].b[13].pco=65535
  // we save ram by not storing the page number text

  if( page >= _cachePageCount )
  {
    debug.printLn(HMI,String(F("Cache not stored for high-order page: ")) + page );
    return;
  }

  String object=getSubtringField(cmd,'=',0);
  String value=getSubtringField(cmd,'=',1);

  int objectOffset = 0;
  char buffer[_cacheBufferSize];

  if( cmd.charAt(4)==']' )
  { // page 10-99
    objectOffset=6;
  }
  else
  { // page 0-9
    objectOffset=5;
  }
  String pageFree =object.substring(objectOffset);
  if( pageFree.startsWith("b[") && (pageFree.charAt(3)==']' || pageFree.charAt(4)==']'))
  {
    int tgtButton;
    // who wants to bet there is a cleaner way to turn b[0] to b[99] into integer 0 to 99?
    if(pageFree.charAt(3)==']')
    {
      tgtButton=(pageFree.charAt(2)-'0');
      objectOffset=5;
    }
    else
    {
      tgtButton=((pageFree.charAt(2)-'0')*10) + (pageFree.charAt(3)-'0');
      objectOffset=6;
    }
    if(tgtButton < _cacheButtonCount )
    { // hokay, we have a button we can cache
      String buttonFree =pageFree.substring(objectOffset);
      if( buttonFree.startsWith("txt"))
      {
        _setCachedTxt(page, tgtButton, value.c_str());
        return;
      }
      if( buttonFree.startsWith("font"))
      {
        _setCachedFont(page, tgtButton, value.toInt());
        return;
      }
      if( buttonFree.startsWith("pco2"))
      { // nb: pco2 first, as startsWith "pco" matches both
        _setCachedPCO2(page, tgtButton, value.toInt());
        return;
      }
      if( buttonFree.startsWith("bco2"))
      { // nb: bco2 first, as startsWith "bco" matches both
        _setCachedBCO2(page, tgtButton, value.toInt());
        return;
      }
      if( buttonFree.startsWith("pco"))
      { // nb: pco second
        _setCachedPCO(page, tgtButton, value.toInt());
        return;
      }
      if( buttonFree.startsWith("bco"))
      { // nb: bco second
        _setCachedBCO(page, tgtButton, value.toInt());
        return;
      }
      if( buttonFree.startsWith("xcen"))
      {
        _setCachedXcen(page, tgtButton, value.toInt());
        return;
      }
      debug.printLn(HMI,String(F("Internal: [DEBUG] input token was not loaded into new cache. Old Cache takes over >>")) + cmd + String(F("<<")));
      // so, no matches found, fall down to the legacy code
    }
  }

  //  debug.printLn(HMI,String(F("--- debug ---  ")) + pageFree + "   " + value);

  // Legacy Cache using JSON that sometimes fails:
  // In the _pageCache[page], find pageFree entry and if it exists, replace it with pageFree+"="+value. If it does not exist, add it.
  //StaticJsonDocument<_cacheBufferSize> cacheCommands;
  DynamicJsonDocument cacheCommands(_cacheBufferSize);
  if( _pageCache[page] != NULL)
  {
    DeserializationError jsonError = deserializeJson(cacheCommands, _pageCache[page]);
    if (jsonError)
    { // Couldn't parse incoming JSON command
      debug.printLn(HMI,String(F("Internal: [ERROR] Failed to update cache. Reported error: ")) + String(jsonError.c_str()));
      debug.printLn(HMI,String(F("Internal: [DEBUG] Input String was: >>")) + cmd + String(F("<<, cache was >>")) + _pageCache[page] + String(F("<<")));
      //
      if( _pageCache[page] != NULL )
      { // fragment memory!
        free(_pageCache[page]);
        _pageCache[page]=NULL;
        _pageCacheLen[page]=0;
      }
      return;
    }
  }
  cacheCommands[pageFree]=value;
  int count=serializeJson(cacheCommands, buffer, _cacheBufferSize-1);
  buffer[_cacheBufferSize-1]='\0'; // force null termination
  int buflen=strlen(buffer);

  if( count > 0 && buflen > _pageCacheLen[page] )
  { // including when _pageCacheLen[page]==0
    if( _pageCache[page] == NULL )
    { // new malloc
      _pageCache[page]=(char*)malloc( sizeof(char) * (buflen+1) );
      if(_pageCache[page] == NULL)
      { // oops
        _pageCacheLen[page]=0;
        debug.printLn(HMI,String(F("Internal: [ERROR] Failed to malloc cache, wanted "))+buflen);
        return;
      }
    }
    else
    { // realloc
      _pageCache[page]=(char*)realloc((void*)_pageCache[page], sizeof(char) * (buflen+1));
      if(_pageCache[page] == NULL)
      { // oops
        _pageCacheLen[page]=0;
        debug.printLn(HMI,String(F("Internal: [ERROR] Failed to realloc cache, was ")) + _pageCacheLen[page]);
        return;
      }
    }
    _pageCacheLen[page]=buflen+1;
  }

  if( count > 0 )
  {
    strncpy(_pageCache[page],buffer,buflen);
    _pageCache[page][buflen]='\0'; // paranoia, force null termination
  }

  if( page == 1 )
  { // More Debug
    //    debug.printLn(HMI,String(F("---  ")) + _pageCache[page]);
    debug.printLn(HMI,String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );
  }
  // and garbage collect
  //cacheCommands.clear();
#endif // NEXTION_CACHE_ENABLED
}

#if NEXTION_CACHE_ENABLED==(true)

////////////////////////////////////////////////////////////////////////////////////////////////////
// already declared somewhere in Arduino. Neat!
//#define BIT(x) (1UL<<(x))

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::_isCachedFontValid(uint8_t page, uint8_t button)
{
  // depend on C short-cut evaluation; if the range checks are false
  // then the array dereference is never attempted
  if( page >= _cachePageCount || button >= _cacheButtonCount || 0 == (_cache_has[page].font & BIT(button)) )
  {
    return false;
  }
  else
  {
    return true;
  }
}
uint8_t hmiNextionClass::_getCachedFont(uint8_t page, uint8_t button)
{
  if( !_isCachedFontValid(page,button) )
  {
    return 6; // we do not have a defined "no font here" value? 6 is a nice proportional font, it will do.
  }
  return _cached[page][button].font;
}
void hmiNextionClass::_setCachedFont(uint8_t page, uint8_t button, uint8_t newFont)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount ) { return; } // no
  // bounds check? Gerard has 10 fonts loaded today
  _cache_has[page].font |= BIT(button);
  _cached[page][button].font=newFont;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::_isCachedXcenValid(uint8_t page, uint8_t button)
{
  // depend on C short-cut evaluation; if the range checks are false
  // then the array dereference is never attempted
  if( page >= _cachePageCount || button >= _cacheButtonCount || 0 == (_cache_has[page].font & BIT(button)) )
  {
    return false;
  }
  else
  {
    return true;
  }
}
uint8_t hmiNextionClass::_getCachedXcen(uint8_t page, uint8_t button)
{
  if( !_isCachedXcenValid(page,button) )
  {
    return 0; // we do not have a defined "no xcen here" value? "0 left aligned" will do
  }
  return _cached[page][button].xcen;
}
void hmiNextionClass::_setCachedXcen(uint8_t page, uint8_t button, uint8_t newXcen)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount ) { return; } // no
  // bounds check?
  _cache_has[page].xcen |= BIT(button);
  _cached[page][button].xcen=newXcen;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::_isCachedPCOValid(uint8_t page, uint8_t button)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount || 0 == (_cache_has[page].pco & BIT(button)) )
  {
    return false;
  }
  else
  {
    return true;
  }
}
uint16_t hmiNextionClass::_getCachedPCO(uint8_t page, uint8_t button)
{
  if( !_isCachedPCOValid(page,button) )
  {
    return 59164; // all colours are legal, so pick one. 59164 = RGB(0xe0,0xe0,0xe0) grey
  }
  return _cached[page][button].pco;
}
void hmiNextionClass::_setCachedPCO(uint8_t page, uint8_t button, uint16_t newColour)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount ) { return; } // no
  // all 65536 colours are legal, so no bounds check here
  _cache_has[page].pco |= BIT(button);
  _cached[page][button].pco=newColour;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::_isCachedBCOValid(uint8_t page, uint8_t button)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount || 0 == (_cache_has[page].bco & BIT(button)) )
  {
    return false;
  }
  else
  {
    return true;
  }
}
uint16_t hmiNextionClass::_getCachedBCO(uint8_t page, uint8_t button)
{
  if( !_isCachedBCOValid(page,button) )
  {
    return 8; // all colours are legal, so pick one. 8 = RGB(0x0,0x0,0x40) midnightBlue
  }
  return _cached[page][button].bco;
}
void hmiNextionClass::_setCachedBCO(uint8_t page, uint8_t button, uint16_t newColour)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount ) { return; } // no
  _cache_has[page].bco |= BIT(button);
  _cached[page][button].bco=newColour;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::_isCachedPCO2Valid(uint8_t page, uint8_t button)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount || 0 == (_cache_has[page].pco2 & BIT(button)) )
  {
    return false;
  }
  else
  {
    return true;
  }
}
uint16_t hmiNextionClass::_getCachedPCO2(uint8_t page, uint8_t button)
{
  if( !_isCachedPCO2Valid(page,button) )
  {
    return 59164; // all colours are legal, so pick one. 59164 = RGB(0xe0,0xe0,0xe0) grey
  }
  return _cached[page][button].pco2;

}
void hmiNextionClass::_setCachedPCO2(uint8_t page, uint8_t button, uint16_t newColour)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount ) { return; } // no
  _cache_has[page].pco2 |= BIT(button);
  _cached[page][button].pco2=newColour;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::_isCachedBCO2Valid(uint8_t page, uint8_t button)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount || 0 == (_cache_has[page].bco2 & BIT(button)) )
  {
    return false;
  }
  else
  {
    return true;
  }
}
uint16_t hmiNextionClass::_getCachedBCO2(uint8_t page, uint8_t button)
{
  if( !_isCachedBCOValid(page,button) )
  {
    return 8; // all colours are legal, so pick one. 8 = RGB(0x0,0x0,0x40) midnightBlue
  }
  return _cached[page][button].bco2;
}
void hmiNextionClass::_setCachedBCO2(uint8_t page, uint8_t button, uint16_t newColour)
{
  if( page >= _cachePageCount || button >= _cacheButtonCount ) { return; } // no
  _cache_has[page].bco2 |= BIT(button);
  _cached[page][button].bco2=newColour;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool hmiNextionClass::_isCachedTxtValid(uint8_t page, uint8_t button)
{
  // dereference the pointer last eh? fewer sigsegv is happier sigsegv
  if( page >= _cachePageCount || button >= _cacheButtonCount || 0 == (_cache_has[page].txt & BIT(button)) || 0 == _cached[page][button].txtlen || NULL == _cached[page][button].txt )
  {
    return false;
  }
  else
  {
    return true;
  }
}
char *hmiNextionClass::_getCachedTxt(uint8_t page, uint8_t button)
{
  if( !_isCachedTxtValid(page,button) )
  {
    return NULL; // explicitly not valid
  }
  return _cached[page][button].txt;
}

bool hmiNextionClass::_helperTxtMalloc(uint8_t page, uint8_t button, const char *newText)
{
  uint8_t newLen = strlen(newText);
  if( newLen < 8 )
  {
  _cached[page][button].txtlen = 8;
  }
  else if( newLen < 250 )
  {
    _cached[page][button].txtlen = 16 * ((newLen/16)+1);
  }
  else
  {
    debug.printLn(HMI,String(F("NMI Cache: Unable to handle request for overly long .txt field! Given length ")) + String(newLen) );
    return false;
  }
  //debug.printLn(HMI,String(F("Internal: [DEBUG] Going to malloc .txt length ")) + _cached[page][button].txtlen);
  _cached[page][button].txt = (char*) malloc( sizeof(char) * _cached[page][button].txtlen );
  if( NULL == _cached[page][button].txt )
  {
    debug.printLn(HMI,String(F("Internal: [ERROR] Failed to malloc .txt cache, wanted ")) + _cached[page][button].txtlen);
    _cached[page][button].txtlen=0;
    // do we unset _cache_has bit too?
    return false;
  }
  return true;
}

void hmiNextionClass::_setCachedTxt(uint8_t page, uint8_t button, const char *newText)
{
  // fragment the RAM!
  // so, we cannot use this function to zero a string by passing NULL
  // instead, pass the empty string - "", a valid pointer dereferencing length zero.
  if( page >= _cachePageCount || button >= _cacheButtonCount || NULL == newText ) { return; } // no

  if( 0 == _cached[page][button].txtlen || NULL == _cached[page][button].txt )
  { // no existing string, malloc a new one
    //debug.printLn(HMI,String(F("Internal: [DEBUG] cache says new txt to ")) + strlen(newText));
    _helperTxtMalloc(page,button,newText);
  }
  else if( _cached[page][button].txtlen < strlen(newText) )
  { // existing string but it is too short, so fragment the ram
    //debug.printLn(HMI,String(F("Internal: [DEBUG] cache says grow txt from ")) + _cached[page][button].txtlen + String(F(" to ")) + strlen(newText));
    free(_cached[page][button].txt);
    _cached[page][button].txt=NULL;
    _cached[page][button].txtlen=0;

    _helperTxtMalloc(page,button,newText);
  }
  else
  {
    //debug.printLn(HMI,String(F("Internal: [DEBUG] cache says keep txt, from ")) + _cached[page][button].txtlen + String(F(" holds ")) + strlen(newText));
    // existing string and it has space to hold the new text
  }

  // Paranoia
  // (actually, if the malloc failed, we might get here)
  if( NULL == _cached[page][button].txt )
  {
    debug.printLn(HMI,String(F("Internal: [ERROR] .txt cache NULL at a place where it really should not be.")));
    return;
  }

  _cache_has[page].txt |= BIT(button);
  strncpy(_cached[page][button].txt, newText, strlen(newText));
  // enforce NUL-termination
  _cached[page][button].txt[_cached[page][button].txtlen-1] = '\0';
  _cached[page][button].txt[strlen(newText)] = '\0';
}

#endif // NEXTION_CACHE_ENABLED
