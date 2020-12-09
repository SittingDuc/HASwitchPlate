// Split settings.h file out
// so I can store secrets here
// -- Gerard, 2020
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)

// This file is only #included once, mmkay
#pragma once
// CLEAN: FOR GITHUB

////////////////////////////////////////////////////////////////////////////////////////////////////
// OPTIONAL: Assign default values here.
#define DEFAULT_WIFI_SSID ("") // Leave unset for wireless autoconfig. Note that these values will be lost

#define DEFAULT_WIFI_PASS ("") // when updating, but that's probably OK because they will be saved in EEPROM.

////////////////////////////////////////////////////////////////////////////////////////////////////
// These defaults may be overwritten with values saved by the web interface
// Note that MQTT prefers dotted quad address, but MQTTS prefers fully qualified domain names (fqdn)
// Note that MQTTS works best using NTP to obtain Time
#define DEFAULT_MQTT_SERVER ("")
#define DEFAULT_MQTT_PORT ("1883")
#define DEFAULT_MQTT_USER ("")
#define DEFAULT_MQTT_PASS ("")
#define DEFAULT_HASP_NODE ("plate01")
#define DEFAULT_GROUP_NAME ("plates")
#define DEFAULT_CONFIG_USER ("admin")
#define DEFAULT_CONFIG_PASS ("")
#define DEFAULT_MOTION_PIN ("0")

////////////////////////////////////////////////////////////////////////////////////////////////////

#define ASECOND (1000)
#define AMINUTE (60*ASECOND)
#define ANHOUR (3600*ASECOND)

////////////////////////////////////////////////////////////////////////////////////////////////////

#define HASP_VERSION (0.40)               // Current HASP software release version
#define WIFI_CONFIG_PASSWORD ("hasplate") // First-time config WPA2 password
#define WIFI_CONFIG_AP ("HASwitchPlate")  // First-time config WPA2 password
#define CONNECTION_TIMEOUT (300)          // Timeout for WiFi and MQTT connection attempts in seconds
#define RECONNECT_TIMEOUT (15)            // Timeout for WiFi reconnection attempts in seconds
#define UPDATE_CHECK_INTERVAL (12*ANHOUR); // Time in msec between update checks (12 hours)

#define NEXTION_REPORT_PAGE0 (false)       // If false, don't report page 0 sendme
#define NEXTION_RETRY_MAX (5)              // Attempt to connect to panel this many times
#define NEXTION_CHECK_INTERVAL (5*ASECOND) // Time in msec between nextion connection checks
#define NEXTION_RESET_PIN (D6)             // Pin for Nextion power rail switch (GPIO12/D6)

#define MQTT_MAX_PACKET_SIZE (4096)             // Size of buffer for incoming MQTT message
#define MQTT_STATUS_UPDATE_INTERVAL (5*AMINUTE) // Time in msec between publishing MQTT status updates (5 minutes)

#define MOTION_LATCH_TIMEOUT (30*ASECOND) // Latch time for motion sensor
#define MOTION_BUFFER_TIMEOUT (1*ASECOND) // Latch time for motion sensor

#define BEEP_DEFAULT_TIME (1*ASECOND) // milliseconds of on-time and off-time for beep
#define BEEP_PIN (D2)                 // Pin for the Speaker, if fitted (GPIO4/D2). Must be AnalogWrite capable.
#define BEEP_ENABLED (false)          // if true, enable the Speaker on BEEP_PIN

#define DEBUG_HMI_VERBOSE (true)     // set false to have fewer printf from HMI/Nextion
#define DEBUG_MQTT_VERBOSE (true)    // set false to have fewer printf from MQTT
#define DEBUG_TELNET_ENABLED (false) // Enable telnet debug output
#define DEBUG_SERIAL_ENABLED (true)  // Enable USB serial debug output

//#define FREE2(A) if( (A) != NULL ) { free(A); (A)=NULL;}
