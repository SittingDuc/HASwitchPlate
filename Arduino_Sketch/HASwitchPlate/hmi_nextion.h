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
extern bool beepEnabled;                           // Keypress beep enabled
extern unsigned long beepPrevMillis;                   // will store last time beep was updated
extern unsigned long beepOnTime;                    // milliseconds of on-time for beep
extern unsigned long beepOffTime;                   // milliseconds of off-time for beep
extern boolean beepState;                                  // beep currently engaged
extern unsigned int beepCounter;                           // Count the number of beeps
extern byte beepPin;                                       // define beep pin output
extern uint8_t nextionResetPin;                       // Pin for Nextion power rail switch (GPIO12/D6)

extern uint32_t tftFileSize;                           // Filesize for TFT firmware upload

extern MQTTClient mqttClient;
extern ESP8266WebServer webServer;
extern void mqttStatusUpdate();
extern bool updateCheck();
extern void espReset();



// Ours. But can't be inside the class?
static const uint8_t maxCacheCount = 14;
static const uint16_t maxCacheSize = 2100; // 2047; // 18 of 3000 does crash = softloop. 13 of 2600 does too


class hmiNextionClass {
  private:    
  public:
    // constructor
    hmiNextionClass(void) { _alive = false; }
    ~hmiNextionClass(void) { _alive = false; }

    const byte Suffix[3] = {0xFF, 0xFF, 0xFF};    // Standard suffix for Nextion commands

    // called on setup to initialise all our things
    void begin(void) {
        _alive=true;
        startupCompleteFlag=false;
        CheckTimer=0;
        RetryMax = NEXTION_RETRY_MAX;
        _lcdConnected=false;
        lcdVersionQueryFlag = false;
        lcdVersion = 0;
        ReturnIndex = 0;
        ActivePage = 0;
        ReportPage0 = NEXTION_REPORT_PAGE0;

        // setting the cache up goes here too
        for( int idx=0;idx<maxCacheCount;idx++){
          pageCache[idx]=NULL; // null terminate each cache at power-on
          pageCacheLen[idx]=0;
        }
        
        while (!_lcdConnected && (millis() < 5000))
        { // Wait up to 5 seconds for serial input from LCD
          HandleInput();
        }
        if (_lcdConnected)
        {
          debug.printLn(F("HMI: LCD responding, continuing program load"));
            SendCmd("connect");
        }
        else
        {
          debug.printLn(F("HMI: LCD not responding, continuing program load"));
        }
    }

    // called in loop to maintain all our things
    void loop(void) {
      if (HandleInput())
      { // Process user input from HMI
        ProcessInput();
      }

      if ((lcdVersion < 1) && (millis() <= (RetryMax * CheckInterval)))
      { // Attempt to connect to LCD panel to collect model and version info during startup
        Connect();
      }
      else if ((lcdVersion > 0) && (millis() <= (RetryMax * CheckInterval)) && !startupCompleteFlag)
      { // We have LCD info, so trigger an update check + report
        if (updateCheck())
        { // Send a status update if the update check worked
          mqttStatusUpdate();
          startupCompleteFlag = true;
        }
      }
      else if ((millis() > (RetryMax * CheckInterval)) && !startupCompleteFlag)
      { // We still don't have LCD info so go ahead and run the rest of the checks once at startup anyway
        updateCheck();
        mqttStatusUpdate();
        startupCompleteFlag = true;
      }

    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void Connect()
    {
        if ((millis() - CheckTimer) >= CheckInterval)
        {
            static unsigned int RetryCount = 0;
            if ((Model.length() == 0) && (RetryCount < (RetryMax - 2)))
            { // Try issuing the "connect" command a few times
            debug.printLn(HMI, F("HMI: sending Nextion connect request"));
            SendCmd("connect");
            RetryCount++;
            CheckTimer = millis();
            }
            else if ((Model.length() == 0) && (RetryCount < RetryMax))
            { // If we still don't have model info, try to change nextion serial speed from 9600 to 115200
            SetSpeed();
            RetryCount++;
            debug.printLn(HMI, F("HMI: sending Nextion serial speed 115200 request"));
            CheckTimer = millis();
            }
            else if ((lcdVersion < 1) && (RetryCount <= RetryMax))
            {
            if (Model.length() == 0)
            { // one last hail mary, maybe the serial speed is set correctly now
                SendCmd("connect");
            }
            SendCmd("get " + lcdVersionQuery);
            lcdVersionQueryFlag = true;
            RetryCount++;
            debug.printLn(HMI, F("HMI: sending Nextion version query"));
            CheckTimer = millis();
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void Reset()
    {
      debug.printLn(F("HMI: Rebooting LCD"));
      digitalWrite(nextionResetPin, LOW);
      Serial1.print("rest"); // yes "rest", not "reset"
      Serial1.write(Suffix, sizeof(Suffix));
      Serial1.flush();
      delay(100);
      digitalWrite(nextionResetPin, HIGH);

      unsigned long lcdResetTimer = millis();
      const unsigned long lcdResetTimeout = 5000;

      _lcdConnected = false;
      while (!_lcdConnected && (millis() < (lcdResetTimer + lcdResetTimeout)))
      {
        HandleInput();
      }
      if (_lcdConnected)
      {
        debug.printLn(F("HMI: Rebooting LCD completed"));
        if (ActivePage)
        {
          SendCmd("page " + String(ActivePage));
        }
      }
      else
      {
        debug.printLn(F("ERROR: Rebooting LCD completed, but LCD is not responding."));
      }
      mqttClient.publish(mqttStatusTopic, "OFF");
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void ReplayCmd(void)
    {
    //  debug.printLn(HMI,String(F("--- Entry Point Replay Cmd ")) + ActivePage);
    //  if( 1 == ActivePage) {
    //    debug.printLn(HMI,String(F("--- Contents  ")) + pageCache[ActivePage]);
    //  }
      if( ActivePage >= maxCacheCount ) {
        debug.printLn(HMI, String(F("Cache cannot replay for high-order page: ")) + ActivePage );
        return;
      }
      // Do nothing if the cache is empty
      if( pageCache[ActivePage] == NULL ) {
        return;
      }
      // Do something if the cache is not.
      //StaticJsonDocument<maxCacheSize> replayCommands;
      DynamicJsonDocument replayCommands(maxCacheSize);
      DeserializationError jsonError = deserializeJson(replayCommands, pageCache[ActivePage]);
      if (jsonError)
      { // Couldn't parse incoming JSON command
        debug.printLn(HMI,String(F("HMI: [ERROR] Failed to replay cache on page change. Reported error: ")) + String(jsonError.c_str()));
      }
      else
      {
        // we saved ram by not storing the p[x]. text, so re-add it here
        // can we do this with printf?
        String preface=String("p[") + ActivePage + String("].");
        JsonObject replayObj = replayCommands.as<JsonObject>();
        //JsonArray replayArray = replayCommands.as<JsonArray>();
    //    if( 1 == ActivePage ) {
    //      debug.printLn(HMI,String(F("--- ah wea  ")) + replayCommands.size() + "  " + replayObj.size() + "  " + preface);
    //    }
        for (JsonPair keyValue : replayObj) {
          //String subsequent = replayCommands[idx];
          String thiskey=keyValue.key().c_str();
          String thisvalue=keyValue.value().as<char*>();
          String resultant = preface + thiskey + "=" + thisvalue;
    //      debug.printLn(HMI,String(F("HMI:  ")) + " " + resultant); // _SendCmd has a printf itself
          _SendCmd(resultant);
          delayMicroseconds(200); // Larger JSON objects can take a while to run through over serial,
        }                         // give the ESP and Nextion a moment to deal with life
      }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void appendCmd(int page, String Cmd)
    {
    //  debug.printLn(HMI,String(F("aCmd ")) + page + "  " + Cmd);
      // our input is like p[1].b[1].txt="Hello World"
      // or p[20].b[13].pco=65535
      // we save ram by not storing the page number text

      if( page >= maxCacheCount ) {
        debug.printLn(HMI,String(F("Cache not stored for high-order page: ")) + page );
        return;
      }

      String object=getSubtringField(Cmd,'=',0);
      String value=getSubtringField(Cmd,'=',1);

      int nom = 0;
      char buffer[maxCacheSize];

      if( Cmd.charAt(4)==']' ) { // page 10-99
        nom=6;
      } else { // page 0-9
        nom=5;
      }
      String pageFree =object.substring(nom);

    //  debug.printLn(HMI,String(F("--- debug ---  ")) + pageFree + "   " + value);

      // In the pageCache[page], find pageFree entry and if it exists, replace it with pageFree+"="+value. If it does not exist, add it.
      //StaticJsonDocument<maxCacheSize> cacheCommands;
      DynamicJsonDocument cacheCommands(maxCacheSize);
      if( pageCache[page] != NULL) {
        DeserializationError jsonError = deserializeJson(cacheCommands, pageCache[page]);
        if (jsonError)
        { // Couldn't parse incoming JSON command
          debug.printLn(HMI,String(F("Internal: [ERROR] Failed to update cache. Reported error: ")) + String(jsonError.c_str()));
          debug.printLn(HMI,String(F("Internal: [DEBUG] Input String was: >>")) + Cmd + String(F("<<, cache was >>")) + pageCache[page] + String(F("<<")));
          //
          if( pageCache[page] != NULL ) { // fragment memory!
            free(pageCache[page]);
            pageCache[page]=NULL;
            pageCacheLen[page]=0;
          }
          return;
        }
      }
      cacheCommands[pageFree]=value;
      int count=serializeJson(cacheCommands, buffer, maxCacheSize-1);
      buffer[maxCacheSize-1]='\0'; // force null termination
      int buflen=strlen(buffer);

      if( count > 0 && buflen > pageCacheLen[page] ) { // including when pageCacheLen[page]==0
        if( pageCache[page] == NULL ) { // new malloc
          pageCache[page]=(char*)malloc( sizeof(char) * (buflen+1) );
          if(pageCache[page] == NULL) { // oops
            pageCacheLen[page]=0;
            debug.printLn(HMI,String(F("Internal: [ERROR] Failed to malloc cache, wanted "))+buflen);
            return;
          }
        } else { // realloc
          pageCache[page]=(char*)realloc((void*)pageCache[page], sizeof(char) * (buflen+1));
          if(pageCache[page] == NULL) { // oops
            pageCacheLen[page]=0;
            debug.printLn(HMI,String(F("Internal: [ERROR] Failed to realloc cache, was ")) + pageCacheLen[page]);
            return;
          }
        }
        pageCacheLen[page]=buflen+1;
      }

      if( count > 0 ) {
        strncpy(pageCache[page],buffer,buflen);
        pageCache[page][buflen]='\0'; // paranoia, force null termination
      }

      if( page == 1 ) {
    //    debug.printLn(HMI,String(F("---  ")) + pageCache[page]);
      debug.printLn(HMI,String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );
      }
      // and garbage collect
      //cacheCommands.clear();
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void SetAttr(String hmiAttribute, String hmiValue)
    { // Set the value of a Nextion component attribute
      SendCmd(hmiAttribute + "=" + hmiValue);
      /*
      Serial1.print(hmiAttribute);
      Serial1.print("=");
      Serial1.print(hmiValue);
      Serial1.write(Suffix, sizeof(Suffix));
      debug.printLn(HMI,String(F("HMI OUT: '")) + hmiAttribute + "=" + hmiValue + "'");
      */
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void GetAttr(String hmiAttribute)
    { // Get the value of a Nextion component attribute
      // This will only send the command to the panel requesting the attribute, the actual
      // return of that value will be handled by ProcessInput and placed into mqttGetSubtopic
      Serial1.print("get " + hmiAttribute);
      Serial1.write(Suffix, sizeof(Suffix));
      debug.printLn(HMI,String(F("HMI OUT: 'get ")) + hmiAttribute + "'");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void SendCmd(String Cmd)
    { // maybe send a commmand to the panel
      // p[1].b[1].text="Hello World"
      if( Cmd.startsWith("p[") && (Cmd.charAt(3)==']' || Cmd.charAt(4)==']')) {
        int tgtPage;
        // who wants to bet there is a cleaner way to turn p[0] to p[99] into integer 0 to 99?
        if(Cmd.charAt(3)==']') {  tgtPage=(Cmd.charAt(2)-'0');
        } else {  tgtPage=((Cmd.charAt(2)-'0')*10) + (Cmd.charAt(3)-'0');
        }

        // Always add the entry to the list
        appendCmd(tgtPage,Cmd);

        // but only render it if the page is active, or the page is global
        if (tgtPage == ActivePage || pageIsGlobal[tgtPage]) {
          _SendCmd(Cmd);
        } // else {
          //debug.printLn(HMI,String(F("HMI Skip: ")) + Cmd);
        //}

      } else {
        // not a button / attribute, send it through
        _SendCmd(Cmd);
      }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void ParseJson(String &strPayload)
    { // Parse an incoming JSON array into individual Nextion commands
      if (strPayload.endsWith(",]"))
      { // Trailing null array elements are an artifact of older Home Assistant automations and need to
        // be removed before parsing by ArduinoJSON 6+
        strPayload.remove(strPayload.length() - 2, 2);
        strPayload.concat("]");
      }
      DynamicJsonDocument Commands(mqttMaxPacketSize + 1024);
      DeserializationError jsonError = deserializeJson(Commands, strPayload);
      if (jsonError)
      { // Couldn't parse incoming JSON command
        debug.printLn(HMI,String(F("MQTT: [ERROR] Failed to parse incoming JSON command with error: ")) + String(jsonError.c_str()));
      }
      else
      {
        for (uint8_t i = 0; i < Commands.size(); i++)
        {
          SendCmd(Commands[i]);
          delayMicroseconds(200); // Larger JSON objects can take a while to run through over serial,
        }                         // give the ESP and Nextion a moment to deal with life
      }
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void SetSpeed()
    {
      debug.printLn(HMI,F("HMI: No Nextion response, attempting 9600bps connection"));
      Serial1.begin(9600);
      Serial1.write(Suffix, sizeof(Suffix));
      Serial1.print("bauds=115200");
      Serial1.write(Suffix, sizeof(Suffix));
      Serial1.flush();
      Serial1.begin(115200);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool HandleInput()
    { // Handle incoming serial data from the Nextion panel
      // This will collect serial data from the panel and place it into the global buffer
      // ReturnBuffer[ReturnIndex]
      // Return: true if we've received a string of 3 consecutive 0xFF values
      // Return: false otherwise
      bool CommandComplete = false;
      static int TermByteCnt = 0;   // counter for our 3 consecutive 0xFFs
      static String hmiDebug = "HMI IN: "; // assemble a string for debug output

      if (Serial.available())
      {
        _lcdConnected = true;
        byte CommandByte = Serial.read();
        hmiDebug += (" 0x" + String(CommandByte, HEX));
        // check to see if we have one of 3 consecutive 0xFF which indicates the end of a command
        if (CommandByte == 0xFF)
        {
          TermByteCnt++;
          if (TermByteCnt >= 3)
          { // We have received a complete command
            CommandComplete = true;
            TermByteCnt = 0; // reset counter
          }
        }
        else
        {
          TermByteCnt = 0; // reset counter if a non-term byte was encountered
        }
        ReturnBuffer[ReturnIndex] = CommandByte;
        ReturnIndex++;
      }
      if (CommandComplete)
      {
        debug.printLn(HMI, hmiDebug);
        hmiDebug = "HMI IN: ";
      }
      return CommandComplete;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void ProcessInput()
    { // Process incoming serial commands from the Nextion panel
      // Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
      // tl;dr: command byte, command data, 0xFF 0xFF 0xFF

      if (ReturnBuffer[0] == 0x65)
      { // Handle incoming touch command
        // 0x65+Page ID+Component ID+TouchEvent+End
        // Return this data when the touch event created by the user is pressed.
        // Definition of TouchEvent: Press Event 0x01, Release Event 0X00
        // Example: 0x65 0x00 0x02 0x01 0xFF 0xFF 0xFF
        // Meaning: Touch Event, Page 0, Object 2, Press
        String Page = String(ReturnBuffer[1]);
        String ButtonID = String(ReturnBuffer[2]);
        byte ButtonAction = ReturnBuffer[3];

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
          GetAttr("p[" + Page + "].b[" + ButtonID + "].val");
        }
      }
      else if (ReturnBuffer[0] == 0x66)
      { // Handle incoming "sendme" page number
        // 0x66+PageNum+End
        // Example: 0x66 0x02 0xFF 0xFF 0xFF
        // Meaning: page 2
        String Page = String(ReturnBuffer[1]);
        debug.printLn(HMI, String(F("HMI IN: [sendme Page] '")) + Page + "'");
        // if ((ActivePage != Page.toInt()) && ((Page != "0") || ReportPage0))
        if ((Page != "0") || ReportPage0)
        { // If we have a new page AND ( (it's not "0") OR (we've set the flag to report 0 anyway) )
          ActivePage = Page.toInt();
          ReplayCmd();
          String mqttPageTopic = mqttStateTopic + "/page";
          debug.printLn(MQTT, String(F("MQTT OUT: '")) + mqttPageTopic + "' : '" + Page + "'");
          mqttClient.publish(mqttPageTopic, Page);
        }
      }
      else if (ReturnBuffer[0] == 0x67)
      { // Handle touch coordinate data
        // 0X67+Coordinate X High+Coordinate X Low+Coordinate Y High+Coordinate Y Low+TouchEvent+End
        // Example: 0X67 0X00 0X7A 0X00 0X1E 0X01 0XFF 0XFF 0XFF
        // Meaning: Coordinate (122,30), Touch Event: Press
        // issue  command "sendxy=1" to enable this output
        uint16_t xCoord = ReturnBuffer[1];
        xCoord = xCoord * 256 + ReturnBuffer[2];
        uint16_t yCoord = ReturnBuffer[3];
        yCoord = yCoord * 256 + ReturnBuffer[4];
        String xyCoord = String(xCoord) + ',' + String(yCoord);
        byte TouchAction = ReturnBuffer[5];
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
      else if (ReturnBuffer[0] == 0x70)
      { // Handle get string return
        // 0x70+ASCII string+End
        // Example: 0x70 0x41 0x42 0x43 0x44 0x31 0x32 0x33 0x34 0xFF 0xFF 0xFF
        // Meaning: String data, ABCD1234
        String getString;
        for (int i = 1; i < ReturnIndex - 3; i++)
        { // convert the payload into a string
          getString += (char)ReturnBuffer[i];
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
      else if (ReturnBuffer[0] == 0x71)
      { // Handle get int return
        // 0x71+byte1+byte2+byte3+byte4+End (4 byte little endian)
        // Example: 0x71 0x7B 0x00 0x00 0x00 0xFF 0xFF 0xFF
        // Meaning: Integer data, 123
        unsigned long getInt = ReturnBuffer[4];
        getInt = getInt * 256 + ReturnBuffer[3];
        getInt = getInt * 256 + ReturnBuffer[2];
        getInt = getInt * 256 + ReturnBuffer[1];
        String getString = String(getInt);
        debug.printLn(HMI,String(F("HMI IN: [Int Return] '")) + getString + "'");

        if (lcdVersionQueryFlag)
        {
          lcdVersion = getInt;
          lcdVersionQueryFlag = false;
          debug.printLn(HMI,String(F("HMI IN: lcdVersion '")) + String(lcdVersion) + "'");
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
      else if (ReturnBuffer[0] == 0x63 && ReturnBuffer[1] == 0x6f && ReturnBuffer[2] == 0x6d && ReturnBuffer[3] == 0x6f && ReturnBuffer[4] == 0x6b)
      { // Catch 'comok' response to 'connect' command: https://www.itead.cc/blog/nextion-hmi-upload-protocol
        String comokField;
        uint8_t comokFieldCount = 0;
        byte comokFieldSeperator = 0x2c; // ","

        for (uint8_t i = 0; i <= ReturnIndex; i++)
        { // cycle through each byte looking for our field seperator
          if (ReturnBuffer[i] == comokFieldSeperator)
          { // Found the end of a field, so do something with it.  Maybe.
            if (comokFieldCount == 2)
            {
              Model = comokField;
              debug.printLn(HMI,String(F("HMI IN: NextionModel: ")) + Model);
            }
            comokFieldCount++;
            comokField = "";
          }
          else
          {
            comokField += String(char(ReturnBuffer[i]));
          }
        }
      }

      else if (ReturnBuffer[0] == 0x1A)
      { // Catch 0x1A error, possibly from .val query against things that might not support that request
        // 0x1A+End
        // ERROR: Variable name invalid
        // We'll be triggering this a lot due to requesting .val on every component that sends us a Touch Off
        // Just reset mqttGetSubtopic and move on with life.
        mqttGetSubtopic = "";
      }
      ReturnIndex = 0; // Done handling the buffer, reset index back to 0
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void StartOtaDownload(String otaUrl)
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
          HandleInput();
          String lcdOtaNextionCmd = "whmi-wri " + String(lcdOtaFileSize) + ",115200,0";
          debug.printLn(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
          Serial1.print(lcdOtaNextionCmd);
          Serial1.write(Suffix, sizeof(Suffix));
          Serial1.flush();

          if (OtaResponse())
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
                if (OtaResponse())
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
          if ((lcdOtaTransferred == lcdOtaFileSize) && OtaResponse())
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
    bool OtaResponse()
    { // Monitor the serial port for a 0x05 response within our timeout

      unsigned long nextionCommandTimeout = 2000;   // timeout for receiving termination string in milliseconds
      unsigned long nextionCommandTimer = millis(); // record current time for our timeout
      bool otaSuccessVal = false;
      while ((millis() - nextionCommandTimer) < nextionCommandTimeout)
      {
        if (Serial.available())
        {
          byte inByte = Serial.read();
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
    void debug_page_cache(void) {
      debug.printLn(String(F("")));
      for( int idx=0;idx<maxCacheCount;idx++) {
        debug.printLn(String(F("debug [")) + idx + String(F("]=")) + pageCacheLen[idx] );
      }
      debug.printLn(String(F("")));
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8_t getActivePage() {
      return ActivePage;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void setActivePage(uint8_t newPage ) {
      // bounds checks? dependent effects?
      ActivePage=newPage;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void changePage(uint8_t page) {
      if (getActivePage() != page)
      { // Hass likes to send duplicate responses to things like page requests and there are no plans to fix that behavior, so try and track it locally
        ActivePage=page;
        SendCmd("page " + String(ActivePage));
      }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    String getModel() {
      return Model;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void setPageGlobal( uint8_t page, bool newFlag ) {
      if( page < maxCacheCount ) {
        pageIsGlobal[page] = newFlag;
      } else {
        debug.printLn(HMI,String(F("Cache cannot be global for high-order page: ")) + page );
      }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool getLCDConnected() {
      return _lcdConnected;
    }

    unsigned long getLCDVersion() {
      return lcdVersion;
    }


  protected:
    //;
    const unsigned long CheckInterval = NEXTION_CHECK_INTERVAL;        // Time in msec between nextion connection checks
    const String lcdVersionQuery = "p[0].b[2].val";  // Object ID for lcdVersion in HMI
    
    bool _alive;
    bool _lcdConnected;                              // Set to true when we've heard something from the LCD
    bool startupCompleteFlag;                        // Startup process has completed
    unsigned long CheckTimer;                        // Timer for nextion connection checks
    unsigned int RetryMax;                           // Attempt to connect to panel this many times
    bool ReportPage0;                    // If false, don't report page 0 sendme
    unsigned long lcdVersion;                        // Int to hold current LCD FW version number
    unsigned long updateLcdAvailableVersion;         // Int to hold the new LCD FW version number
    bool lcdVersionQueryFlag;                        // Flag to set if we've queried lcdVersion
    String Model;                                    // Record reported model number of LCD panel
    uint8_t ReturnIndex;                             // Index for nextionReturnBuffer
    uint8_t ActivePage;                              // Track active LCD page
    byte ReturnBuffer[128];                   // Byte array to pass around data coming from the panel

    bool pageIsGlobal[maxCacheCount] = {false,};
    char* pageCache[maxCacheCount] = {NULL,};
    uint16_t pageCacheLen[maxCacheCount] = {0,};
    //static char hopeless[maxCacheSize] =  {'\0',};


    void _SendCmd(String Cmd)
    { // Send a raw command to the Nextion panel
        Serial1.print(Cmd);
        Serial1.write(Suffix, sizeof(Suffix));
        debug.printLn(HMI,String(F("HMI OUT: ")) + Cmd);
    }
};
