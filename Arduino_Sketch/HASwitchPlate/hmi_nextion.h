// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// hmi_nextion.h : A class and support works to interact with the Nextion-brand HMI LCD touchscreen
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

#include "settings.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "debug.h"

extern debugClass debug; // our serial debug interface

//extern void debug.printLn(HMI,String debugText); // TODO: class me
//extern void debugPrint(String debugText); // TODO: class me
extern String getSubtringField(String data, char separator, int index); // TODO: class me
extern String printHex8(String data, uint8_t length); // TODO: class me
extern const uint16_t mqttMaxPacketSize; // TODO: class me

// Class These!
extern String mqttClientId;                                // Auto-generated MQTT ClientID
extern String mqttGetSubtopic;                             // MQTT subtopic for incoming commands requesting .val
extern String mqttGetSubtopicJSON;                         // MQTT object buffer for JSON status when requesting .val
extern String mqttStateTopic;                              // MQTT topic for outgoing panel interactions
extern String mqttStateJSONTopic;                          // MQTT topic for outgoing panel interactions in JSON format
extern String mqttCommandTopic;                            // MQTT topic for incoming panel commands
extern String mqttGroupCommandTopic;                       // MQTT topic for incoming group panel commands
extern String mqttStatusTopic;                             // MQTT topic for publishing device connectivity state
extern String mqttSensorTopic;                             // MQTT topic for publishing device information in JSON format
extern String mqttLightCommandTopic;                       // MQTT topic for incoming panel backlight on/off commands
extern String mqttBeepCommandTopic;                        // MQTT topic for error beep
extern String mqttLightStateTopic;                         // MQTT topic for outgoing panel backlight on/off state
extern String mqttLightBrightCommandTopic;                 // MQTT topic for incoming panel backlight dimmer commands
extern String mqttLightBrightStateTopic;                   // MQTT topic for outgoing panel backlight dimmer state
extern String mqttMotionStateTopic;                        // MQTT topic for outgoing motion sensor state

// Class These!
extern bool     beepEnabled;                           // Keypress beep enabled
extern uint32_t beepPrevMillis;                   // will store last time beep was updated
extern uint32_t beepOnTime;                    // milliseconds of on-time for beep
extern uint32_t beepOffTime;                   // milliseconds of off-time for beep
extern bool     beepState;                                  // beep currently engaged
extern uint32_t beepCounter;                           // Count the number of beeps
extern uint8_t  beepPin;                                       // define beep pin output
extern uint8_t  nextionResetPin;                       // Pin for Nextion power rail switch (GPIO12/D6)

extern uint32_t tftFileSize;                           // Filesize for TFT firmware upload

extern MQTTClient mqttClient;
extern ESP8266WebServer webServer;
extern void mqttStatusUpdate();
extern bool updateCheck();
extern void espReset();



// Ours. But can't be inside the class?
static const bool     useCache = false;    // when false, disable all the _pageCache code (be like the Upstream project)
static const uint8_t  maxCacheCount = 14;  // or 18?
static const uint16_t maxCacheSize = 2100; // 2047; // 18 of 3000 does crash = softloop. 13 of 2600 does too


class hmiNextionClass {
  private:
  public:
    // constructor
    hmiNextionClass(void) { _alive = false; }
    // destructor
    ~hmiNextionClass(void)
    {
      _alive = false;
      // Free any memory we alloc'd
      for( int idx=0;idx<maxCacheCount;idx++)
      {
        if( _pageCache[idx] )
        {
          free( _pageCache[idx] );
          _pageCache[idx]=NULL;
          _pageCacheLen[idx]=0;
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // visible constants and variables
    const uint8_t Suffix[3] = {0xFF, 0xFF, 0xFF};    // Standard suffix for Nextion commands

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void begin(void)
    { // called on setup to initialise all our things
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

        // setting the cache up goes here too
        for( int idx=0; idx<maxCacheCount; idx++)
        {
          _pageCache[idx]=NULL; // null terminate each cache at power-on
          _pageCacheLen[idx]=0;
          _pageIsGlobal[idx]=false; // default to local scope
        }
        // any pages that default to GlobalScope could go here too

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
    void loop(void)
    { // called in loop to maintain all our things
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
          mqttStatusUpdate();
          _startupCompleteFlag = true;
        }
      }
      else if ((millis() > (_retryMax * CheckInterval)) && !_startupCompleteFlag)
      { // We still don't have LCD info so go ahead and run the rest of the checks once at startup anyway
        updateCheck();
        mqttStatusUpdate();
        _startupCompleteFlag = true;
      }

    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void reset()
    {
      debug.printLn(F("HMI: Rebooting LCD"));
      digitalWrite(nextionResetPin, LOW);
      Serial1.print("rest"); // yes "rest", not "reset"
      Serial1.write(Suffix, sizeof(Suffix));
      Serial1.flush();
      delay(100);
      digitalWrite(nextionResetPin, HIGH);

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
      mqttClient.publish(mqttStatusTopic, "OFF");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void setAttr(String hmiAttribute, String hmiValue)
    { // Set the value of a Nextion component attribute
      sendCmd(hmiAttribute + "=" + hmiValue);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void getAttr(String hmiAttribute)
    { // Get the value of a Nextion component attribute
      // This will only send the command to the panel requesting the attribute, the actual
      // return of that value will be handled by processInput and placed into mqttGetSubtopic
      Serial1.print("get " + hmiAttribute);
      Serial1.write(Suffix, sizeof(Suffix));
      debug.printLn(HMI,String(F("HMI OUT: 'get ")) + hmiAttribute + "'");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void sendCmd(String cmd)
    {
      if( !useCache )
      { // No cache, just send all commands straight to the panel
        _sendCmd(cmd);
        return;
      }

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
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void parseJson(String &strPayload)
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
    bool handleInput()
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
    void processInput()
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
        String Page = String(_returnBuffer[1]);
        String ButtonID = String(_returnBuffer[2]);
        uint8_t ButtonAction = _returnBuffer[3];

        if (ButtonAction == 0x01)
        {
          debug.printLn(HMI, String(F("HMI IN: [Button ON] 'p[")) + Page + "].b[" + ButtonID + "]'");
          String mqttButtonTopic = mqttStateTopic + "/p[" + Page + "].b[" + ButtonID + "]";
          debug.printLn(MQTT,String(F("MQTT OUT: '")) + mqttButtonTopic + "' : 'ON'");
          mqttClient.publish(mqttButtonTopic, "ON");
          String mqttButtonJSONEvent = String(F("{\"event\":\"p[")) + String(Page) + String(F("].b[")) + String(ButtonID) + String(F("]\", \"value\":\"ON\"}"));
          mqttClient.publish(mqttStateJSONTopic, mqttButtonJSONEvent);
          if (beepEnabled)
          {
            beepOnTime = 500;
            beepOffTime = 100;
            beepCounter = 1;
          }
        }
        if (ButtonAction == 0x00)
        {
          debug.printLn(HMI, String(F("HMI IN: [Button OFF] 'p[")) + Page + "].b[" + ButtonID + "]'");
          String mqttButtonTopic = mqttStateTopic + "/p[" + Page + "].b[" + ButtonID + "]";
          debug.printLn(MQTT, String(F("MQTT OUT: '")) + mqttButtonTopic + "' : 'OFF'");
          mqttClient.publish(mqttButtonTopic, "OFF");
          // Now see if this object has a .val that might have been updated.  Works for sliders,
          // two-state buttons, etc, throws a 0x1A error for normal buttons which we'll catch and ignore
          mqttGetSubtopic = "/p[" + Page + "].b[" + ButtonID + "].val";
          mqttGetSubtopicJSON = "p[" + Page + "].b[" + ButtonID + "].val";
          getAttr("p[" + Page + "].b[" + ButtonID + "].val");
        }
      }
      else if (_returnBuffer[0] == 0x66)
      { // Handle incoming "sendme" page number
        // 0x66+PageNum+End
        // Example: 0x66 0x02 0xFF 0xFF 0xFF
        // Meaning: page 2
        String Page = String(_returnBuffer[1]);
        debug.printLn(HMI, String(F("HMI IN: [sendme Page] '")) + Page + "'");
        // if ((_activePage != Page.toInt()) && ((Page != "0") || _reportPage0))
        if ((Page != "0") || _reportPage0)
        { // If we have a new page AND ( (it's not "0") OR (we've set the flag to report 0 anyway) )
          _activePage = Page.toInt();
          _replayCmd();
          String mqttPageTopic = mqttStateTopic + "/page";
          debug.printLn(MQTT, String(F("MQTT OUT: '")) + mqttPageTopic + "' : '" + Page + "'");
          mqttClient.publish(mqttPageTopic, Page);
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
          String mqttTouchTopic = mqttStateTopic + "/touchOn";
          debug.printLn(MQTT,String(F("MQTT OUT: '")) + mqttTouchTopic + "' : '" + xyCoord + "'");
          mqttClient.publish(mqttTouchTopic, xyCoord);
        }
        else if (TouchAction == 0x00)
        {
          debug.printLn(HMI,String(F("HMI IN: [Touch OFF] '")) + xyCoord + "'");
          String mqttTouchTopic = mqttStateTopic + "/touchOff";
          debug.printLn(MQTT,String(F("MQTT OUT: '")) + mqttTouchTopic + "' : '" + xyCoord + "'");
          mqttClient.publish(mqttTouchTopic, xyCoord);
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
        if (mqttGetSubtopic == "")
        { // If there's no outstanding request for a value, publish to mqttStateTopic
          debug.printLn(MQTT,String(F("MQTT OUT: '")) + mqttStateTopic + "' : '" + getString + "]");
          mqttClient.publish(mqttStateTopic, getString);
        }
        else
        { // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
          String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
          debug.printLn(MQTT,String(F("MQTT OUT: '")) + mqttReturnTopic + "' : '" + getString + "]");
          mqttClient.publish(mqttReturnTopic, getString);
          mqttGetSubtopic = "";
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
        else if (mqttGetSubtopic == "")
        {
          mqttClient.publish(mqttStateTopic, getString);
        }
        // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
        else
        {
          String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
          mqttClient.publish(mqttReturnTopic, getString);
          String mqttButtonJSONEvent = String(F("{\"event\":\"")) + mqttGetSubtopicJSON + String(F("\", \"value\":")) + getString + String(F("}"));
          mqttClient.publish(mqttStateJSONTopic, mqttButtonJSONEvent);
          mqttGetSubtopic = "";
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
        mqttGetSubtopic = "";
      }
      _returnIndex = 0; // Done handling the buffer, reset index back to 0
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void startOtaDownload(String otaUrl)
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
          if (mqttClient.connected())
          {
            debug.printLn(F("LCD OTA: LCD firmware upload starting, closing MQTT connection."));
            mqttClient.publish(mqttStatusTopic, "OFF", true, 1);
            mqttClient.publish(mqttSensorTopic, "{\"status\": \"unavailable\"}", true, 1);
            mqttClient.disconnect();
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
              webServer.handleClient();
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
    bool otaResponse()
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
    void debug_page_cache(void)
    {
      if( !useCache ) { return; }

      debug.printLn(String(F("")));
      for( int idx=0;idx<maxCacheCount;idx++) {
        debug.printLn(String(F("debug [")) + idx + String(F("]=")) + _pageCacheLen[idx] );
      }
      debug.printLn(String(F("")));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline uint8_t getActivePage() { return _activePage; }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void setActivePage(uint8_t newPage )
    {
      // bounds checks? dependent effects?
      _activePage=newPage;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void changePage(uint8_t page) {
      if (getActivePage() != page)
      { // Hass likes to send duplicate responses to things like page requests and there are no plans to fix that behavior, so try and track it locally
        _activePage=page;
        sendCmd("page " + String(_activePage));
      }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline String getModel() { return _model; }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void setPageGlobal( uint8_t page, bool newFlag )
    {
      if( page < maxCacheCount )
      {
        _pageIsGlobal[page] = newFlag;
      }
      else
      {
        debug.printLn(HMI,String(F("Cache cannot be global for high-order page: ")) + page );
      }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline bool getLCDConnected() { return _lcdConnected; }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline uint32_t getLCDVersion() { return _lcdVersion; }

  protected:
    //;
    const uint32_t CheckInterval = NEXTION_CHECK_INTERVAL;        // Time in msec between nextion connection checks
    const String _lcdVersionQuery = "p[0].b[2].val";  // Object ID for lcdVersion in HMI

    bool     _alive;                      // Flag that data structures are initialised and functions can run without error
    bool     _lcdConnected;               // Set to true when we've heard something from the LCD
    bool     _startupCompleteFlag;        // Startup process has completed (subtly different from _alive)
    uint32_t _checkTimer;                 // Timer for nextion connection checks
    uint32_t _retryMax;                   // Attempt to connect to panel this many times
    bool     _reportPage0;                // If false, don't report page 0 sendme
    uint32_t _lcdVersion;                 // Int to hold current LCD FW version number
    uint32_t _updateLcdAvailableVersion;  // Int to hold the new LCD FW version number
    bool     _lcdVersionQueryFlag;        // Flag to set if we've queried lcdVersion
    String   _model;                      // Record reported model number of LCD panel
    uint8_t  _returnIndex;                // Index for nextionreturnBuffer
    uint8_t  _activePage;                 // Track active LCD page
    uint8_t  _returnBuffer[128];          // Byte array to pass around data coming from the panel

    bool     _pageIsGlobal[maxCacheCount]; // when true buttons on page are global-scope, when false they are local-scope
    char*    _pageCache[maxCacheCount];    // malloc'd array holding the JSON of the page
    uint16_t _pageCacheLen[maxCacheCount]; // length of malloc'd array to help avoid (*NULL)
    //static char hopeless[maxCacheSize];  // debugging

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void _sendCmd(String cmd)
    { // Send a raw command to the Nextion panel
        Serial1.print(cmd);
        Serial1.write(Suffix, sizeof(Suffix));
        debug.printLn(HMI,String(F("HMI OUT: ")) + cmd);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void _connect()
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
    void _setSpeed()
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
    void _replayCmd(void)
    { // play entries from the cache to the panel
      if( !useCache ) { return; }
    //  debug.printLn(HMI,String(F("--- Entry Point Replay Cmd ")) + _activePage);
    //  if( 1 == _activePage)
    //  {
    //    debug.printLn(HMI,String(F("--- Contents  ")) + _pageCache[_activePage]);
    //  }
      if( _activePage >= maxCacheCount )
      {
        debug.printLn(HMI, String(F("Cache cannot replay for high-order page: ")) + _activePage );
        return;
      }
      // Do nothing if the cache is empty
      if( _pageCache[_activePage] == NULL )
      {
        return;
      }
      // Do something if the cache is not.
      //StaticJsonDocument<maxCacheSize> replayCommands;
      DynamicJsonDocument replayCommands(maxCacheSize);
      DeserializationError jsonError = deserializeJson(replayCommands, _pageCache[_activePage]);
      if (jsonError)
      { // Couldn't parse incoming JSON command
        debug.printLn(HMI,String(F("HMI: [ERROR] Failed to replay cache on page change. Reported error: ")) + String(jsonError.c_str()));
      }
      else
      {
        // we saved ram by not storing the p[x]. text, so re-add it here
        // can we do this with printf?
        String preface=String("p[") + _activePage + String("].");
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
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void _appendCmd(int page, String cmd)
    { // add entries to the cache, and pass input that is unsupported or for the activePage to the panel too
      if( !useCache ) { return; }
    //  debug.printLn(HMI,String(F("aCmd ")) + page + "  " + cmd);
      // our input is like p[1].b[1].txt="Hello World"
      // or p[20].b[13].pco=65535
      // we save ram by not storing the page number text

      if( page >= maxCacheCount )
      {
        debug.printLn(HMI,String(F("Cache not stored for high-order page: ")) + page );
        return;
      }

      String object=getSubtringField(cmd,'=',0);
      String value=getSubtringField(cmd,'=',1);

      int nom = 0;
      char buffer[maxCacheSize];

      if( cmd.charAt(4)==']' )
      { // page 10-99
        nom=6;
      }
      else
      { // page 0-9
        nom=5;
      }
      String pageFree =object.substring(nom);

    //  debug.printLn(HMI,String(F("--- debug ---  ")) + pageFree + "   " + value);

      // In the _pageCache[page], find pageFree entry and if it exists, replace it with pageFree+"="+value. If it does not exist, add it.
      //StaticJsonDocument<maxCacheSize> cacheCommands;
      DynamicJsonDocument cacheCommands(maxCacheSize);
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
      int count=serializeJson(cacheCommands, buffer, maxCacheSize-1);
      buffer[maxCacheSize-1]='\0'; // force null termination
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
    }

};
