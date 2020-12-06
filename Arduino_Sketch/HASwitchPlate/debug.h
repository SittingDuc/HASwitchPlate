// -*- C++ -*-
// HASwitchPlate Forked
// 
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
// 
//
// debug.h : A class and support works to print debug messages to the serial console
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

#include "settings.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>

extern WiFiClient telnetClient;

enum source_t {
    HMI=0,
    MQTT,
    SYSTEM,
    WIFI
};

class debugClass {
    private:
    public:
    debugClass( void) { _alive = false;};
    ~debugClass(void) { _alive = false;};

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // called on setup to initialise all our things
    void begin(void)
    {
        _verboseDebugHMI    = DEBUG_HMI_VERBOSE;
        _verboseDebugMQTT   = DEBUG_MQTT_VERBOSE;
        _verboseDebugSystem = true;
        _verboseDebugWiFi   = true;
        _telnetEnabled      = DEBUG_TELNET_ENABLED;
        _serialEnabled      = DEBUG_SERIAL_ENABLED;
        _alive              = true;
    }

    inline void enableSerial(bool enable=true)   { _serialEnabled = enable; }
    inline void disableSerial(bool enable=false) { _serialEnabled = enable; }
    inline void enableTelnet(bool enable=true)   { _telnetEnabled = enable; }
    inline void disableTelnet(bool enable=false) { _telnetEnabled = enable; }

    inline bool getSerialEnabled() { return _serialEnabled; }
    inline bool getTelnetEnabled() { return _telnetEnabled; }


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void printLn(String debugText)
    {
        // Debug output line of text to our debug targets
        String debugTimeText = "[+" + String(float(millis()) / 1000, 3) + "s] " + debugText;
        Serial.println(debugTimeText);
        if (_serialEnabled)
        {
            SoftwareSerial debugSerial(-1, 1); // -1==nc for RX, 1==TX pin
            debugSerial.begin(115200);
            debugSerial.println(debugTimeText);
            debugSerial.flush();
        }
        if (_telnetEnabled && telnetClient.connected())
        {
            telnetClient.println(debugTimeText);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void print(String debugText)
    { // Debug output single character to our debug targets (DON'T USE THIS!)
        // Try to avoid using this function if at all possible.  When connected to telnet, printing each
        // character requires a full TCP round-trip + acknowledgement back and execution halts while this
        // happens.  Far better to put everything into a line and send it all out in one packet using
        // debugPrintln.
        Serial.print(debugText);
        if (_serialEnabled)
        {
            SoftwareSerial debugSerial(-1, 1); // -1==nc for RX, 1==TX pin
            debugSerial.begin(115200);
            debugSerial.print(debugText);
            debugSerial.flush();
        }
        if (_telnetEnabled && telnetClient.connected())
        {
            telnetClient.print(debugText);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void printLn( enum source_t source, String debugText )
    { // a wrapper version that might print or might not
        if( source == HMI    && _verboseDebugHMI)    { printLn(debugText); }
        if( source == MQTT   && _verboseDebugMQTT)   { printLn(debugText); }
        if( source == WIFI   && _verboseDebugWiFi)   { printLn(debugText); }
        if( source == SYSTEM && _verboseDebugSystem) { printLn(debugText); }
    }

    // uniquely named wrapper versions
    inline void printLnHMI(    String debugText ) { if( _verboseDebugHMI )    { printLn(debugText); } }
    inline void printLnMQTT(   String debugText ) { if( _verboseDebugMQTT )   { printLn(debugText); } }
    inline void printLnSystem( String debugText ) { if( _verboseDebugSystem ) { printLn(debugText); } }
    inline void printLnWiFi(   String debugText ) { if( _verboseDebugWiFi )   { printLn(debugText); } }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void verbosity( enum source_t source, bool verbose=false)
    { // enable or disable printing blocks
        if( source == HMI)    { _verboseDebugHMI   = verbose; }
        if( source == MQTT)   { _verboseDebugMQTT  = verbose; }
        if( source == WIFI)   { _verboseDebugWiFi  = verbose; }
        if( source == SYSTEM) { _verboseDebugSystem= verbose; }
    }

    ;
    protected:
        bool _alive;              // Flag that data structures are initialised and functions can run without error
        bool _serialEnabled;      // Enable USB serial debug output
        bool _telnetEnabled;      // Enable telnet debug output
        bool _verboseDebugHMI;    // set false to have fewer printf from HMI/Nextion
        bool _verboseDebugMQTT;   // set false to have fewer printf from MQTT
        bool _verboseDebugSystem; // set false to have fewer printf from System
        bool _verboseDebugWiFi;   // set false to have fewer printf from WiFi
};
