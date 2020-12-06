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

    // called on setup to initialise all our things
    void begin(void) {
        verboseDebugHMI=DEBUG_HMI_VERBOSE;
        verboseDebugMQTT=DEBUG_MQTT_VERBOSE;
        verboseDebugSystem=true;
        verboseDebugWiFi=true;
        telnetEnabled=DEBUG_TELNET_ENABLED;
        serialEnabled=DEBUG_SERIAL_ENABLED;
        _alive=true;
    }

    inline void enableSerial(bool enable=true) { serialEnabled=enable; }
    inline void disableSerial(bool enable=false) { serialEnabled=enable; }
    inline void enableTelnet(bool enable=true) { telnetEnabled=enable; }
    inline void disableTelnet(bool enable=false) { telnetEnabled=enable; }

    bool getSerialEnabled() { return serialEnabled; }
    bool getTelnetEnabled() { return telnetEnabled; }


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void printLn(String debugText) {
        // Debug output line of text to our debug targets
        String debugTimeText = "[+" + String(float(millis()) / 1000, 3) + "s] " + debugText;
        Serial.println(debugTimeText);
        if (serialEnabled)
        {
            SoftwareSerial debugSerial(-1, 1); // -1==nc for RX, 1==TX pin
            debugSerial.begin(115200);
            debugSerial.println(debugTimeText);
            debugSerial.flush();
        }
        if (telnetEnabled && telnetClient.connected()) {
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
        if (serialEnabled)
        {
            SoftwareSerial debugSerial(-1, 1); // -1==nc for RX, 1==TX pin
            debugSerial.begin(115200);
            debugSerial.print(debugText);
            debugSerial.flush();
        }
        if (telnetEnabled && telnetClient.connected())
        {
            telnetClient.print(debugText);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void printLn( enum source_t source, String debugText ) {
        if( source == HMI && verboseDebugHMI) { printLn(debugText); }
        if( source == MQTT && verboseDebugMQTT) { printLn(debugText); }
        if( source == WIFI && verboseDebugWiFi) { printLn(debugText); }
        if( source == SYSTEM && verboseDebugSystem) { printLn(debugText); }
    }

    inline void printLnHMI( String debugText ) {
        if( verboseDebugHMI ) { printLn(debugText); }
    }
    inline void printLnMQTT( String debugText ) {
        if( verboseDebugMQTT ) { printLn(debugText); }
    }
    inline void printLnSystem( String debugText ) {
        if( verboseDebugSystem ) { printLn(debugText); }
    }
    inline void printLnWiFi( String debugText ) {
        if( verboseDebugWiFi ) { printLn(debugText); }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void verbosity( enum source_t source, bool verbose=false)
    {
        if( source == HMI) { verboseDebugHMI=verbose; }
        if( source == MQTT) { verboseDebugMQTT=verbose; }
        if( source == WIFI) { verboseDebugWiFi=verbose; }
        if( source == SYSTEM) { verboseDebugSystem=verbose; }
    }

    ;
    protected:
        bool _alive;
        bool serialEnabled;
        bool telnetEnabled;

        bool verboseDebugHMI;
        bool verboseDebugMQTT;
        bool verboseDebugSystem;
        bool verboseDebugWiFi;
};
