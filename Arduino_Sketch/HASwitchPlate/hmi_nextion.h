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

// Ours. But can't be inside the class?
static const bool     useCache = false;    // when false, disable all the _pageCache code (be like the Upstream project)
static const uint8_t  maxCacheCount = 14;  // or 18?
static const uint16_t maxCacheSize = 2100; // 2047; // 18 of 3000 does crash = softloop. 13 of 2600 does too

// 11, 8, 11, 10, 8, 8, 8, 10, 11, 7, 11, 8,
// 7, 11, 15, 8, 13, 18

// 12 pages, 7-11 buttons per page, 40 bytes per button. mmm
// pco .2, bco .2, font .1, text .40(!)
// 45 * 18 * 18 = 14.5kB; 45 * 8 * 12 = 4.3kB;
// do we want a hint from the user?

typedef struct _button_struct {
  // GCC prefers a uint32_t near the top of a struct. we don't have one.
  char *txt;
  uint8_t txtlen; // limit 255 characters!
  uint8_t font;
  uint16_t pco;
  uint16_t bco;
  uint16_t pco2;
  uint16_t bco2;
} button_t; // one per button per page

typedef struct _have_struct {
  uint32_t font;
  uint32_t txt;
  uint32_t pco;
  uint32_t bco;
  uint32_t pco2;
  uint32_t bco2;
} have_t; // one (for all buttons) per page

// bool is usually stored as uint8_t, so pivot and use a uint32_t to store 32 of them.
// Gerard is hand picking these constants for HIS display. This should be made more flexible before uploading to GitHub, eh?
static const uint8_t _cachePageCount = 14;
static const uint8_t _cacheButtonCount = 12;


class hmiNextionClass {
private:
public:
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // constructor
  hmiNextionClass(void)
  {
    _alive = false;
    // set our data structures to zero
    // on esp32 is bzero() more efficient than memset()?
    memset(&_cached, 0x00, sizeof(button_t) * _cachePageCount * _cacheButtonCount );
    memset(&_cache_has, 0x00, sizeof(have_t) * _cachePageCount );
  }

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
  button_t _cached[_cachePageCount][_cacheButtonCount];
  have_t _cache_has[_cachePageCount];
  //static char hopeless[maxCacheSize];  // debugging
  /*
  uint8_t  _cacheFont[cachePageCount][cacheButtonCount];
  uint16_t _cachePCO[cachePageCount][cacheButtonCount];
  uint16_t _cacheBCO[cachePageCount][cacheButtonCount];
  uint16_t _cachePCO2[cachePageCount][cacheButtonCount];
  uint16_t _cacheBCO2[cachePageCount][cacheButtonCount];
  char*    _cacheText[cachePageCount][cacheButtonCount];
  */


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

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool _isCachedFontValid(uint8_t page, uint8_t button);
  uint8_t _getCachedFont(uint8_t page, uint8_t button);
  void _setCachedFont(uint8_t page, uint8_t button, uint8_t newFont);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool _isCachedPCOValid(uint8_t page, uint8_t button);
  uint16_t _getCachedPCO(uint8_t page, uint8_t button);
  void _setCachedPCO(uint8_t page, uint8_t button, uint16_t newColour);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool _isCachedBCOValid(uint8_t page, uint8_t button);
  uint16_t _getCachedBCO(uint8_t page, uint8_t button);
  void _setCachedBCO(uint8_t page, uint8_t button, uint16_t newColour);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool _isCachedPCO2Valid(uint8_t page, uint8_t button);
  uint16_t _getCachedPCO2(uint8_t page, uint8_t button);
  void _setCachedPCO2(uint8_t page, uint8_t button, uint16_t newColour);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool _isCachedBCO2Valid(uint8_t page, uint8_t button);
  uint16_t _getCachedBCO2(uint8_t page, uint8_t button);
  void _setCachedBCO2(uint8_t page, uint8_t button, uint16_t newColour);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool _isCachedTxtValid(uint8_t page, uint8_t button);
  char *_getCachedTxt(uint8_t page, uint8_t button);
  void _setCachedTxt(uint8_t page, uint8_t button, const char *newText);
  bool _helperTxtMalloc(uint8_t page, uint8_t button, const char *newText);
};
