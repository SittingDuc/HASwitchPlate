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
#include <ESP8266WebServer.h> // class ESP8266WebServer. TODO: pack into another class?


//extern void debug.printLn(HMI,String debugText); // TODO: class me
//extern void debugPrint(String debugText); // TODO: class me
extern String getSubtringField(String data, char separator, int index); // TODO: class me
extern String printHex8(String data, uint8_t length); // TODO: class me

// Class These!
extern bool     beepEnabled;                           // Keypress beep enabled
extern uint32_t beepPrevMillis;                        // will store last time beep was updated
extern uint32_t beepOnTime;                            // milliseconds of on-time for beep
extern uint32_t beepOffTime;                           // milliseconds of off-time for beep
extern bool     beepState;                             // beep currently engaged
extern uint32_t beepCounter;                           // Count the number of beeps
extern uint8_t  beepPin;                               // define beep pin output

extern uint32_t tftFileSize;                           // Filesize for TFT firmware upload

extern ESP8266WebServer webServer;
extern bool updateCheck();
extern void espReset();


// Ours. But can't be inside the class?
static const bool     useCache = false;    // when false, disable all the _pageCache code (be like the Upstream project)
static const uint8_t  maxCacheCount = 14;  // or 18?
static const uint16_t maxCacheSize = 2100; // 2047; // 18 of 3000 does crash = softloop. 13 of 2600 does too


class hmiNextionClass {
private:
public:
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // constructor
  hmiNextionClass(void) { _alive = false; }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
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
  void begin(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void loop(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void reset();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // note this might be called before begin()
  void initResetPin();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  inline void setAttr(String hmiAttribute, String hmiValue)
  { // Set the value of a Nextion component attribute
    sendCmd(hmiAttribute + "=" + hmiValue);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void getAttr(String hmiAttribute);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void sendCmd(String cmd);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void parseJson(String &strPayload);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool handleInput();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void processInput();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void startOtaDownload(String otaUrl);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool otaResponse();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void debug_page_cache(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  inline uint8_t getActivePage() { return _activePage; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void setActivePage(uint8_t newPage );

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void changePage(uint8_t page);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  inline String getModel() { return _model; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void setPageGlobal( uint8_t page, bool newFlag );

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  inline bool getLCDConnected() { return _lcdConnected; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  inline uint32_t getLCDVersion() { return _lcdVersion; }

protected:
  //;
  const uint32_t CheckInterval = NEXTION_CHECK_INTERVAL;        // Time in msec between nextion connection checks
  const String _lcdVersionQuery = "p[0].b[2].val";  // Object ID for lcdVersion in HMI

  bool     _alive;                      // Flag that data structures are initialised and functions can run without error
  uint8_t  _resetPin;                   // Pin for Nextion power rail switch (GPIO12/D6)
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
  String   _mqttGetSubtopic;            // MQTT subtopic for incoming commands requesting .val
  String   _mqttGetSubtopicJSON;        // MQTT object buffer for JSON status when requesting .val
    

  bool     _pageIsGlobal[maxCacheCount]; // when true buttons on page are global-scope, when false they are local-scope
  char*    _pageCache[maxCacheCount];    // malloc'd array holding the JSON of the page
  uint16_t _pageCacheLen[maxCacheCount]; // length of malloc'd array to help avoid (*NULL)
  //static char hopeless[maxCacheSize];  // debugging

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _sendCmd(String cmd);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _connect();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _setSpeed();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _replayCmd(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _appendCmd(int page, String cmd);
};
