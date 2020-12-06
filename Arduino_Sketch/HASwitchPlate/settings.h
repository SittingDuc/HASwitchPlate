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
char wifiSSID[32] = ""; // Leave unset for wireless autoconfig. Note that these values will be lost
char wifiPass[64] = ""; // when updating, but that's probably OK because they will be saved in EEPROM.

////////////////////////////////////////////////////////////////////////////////////////////////////
// These defaults may be overwritten with values saved by the web interface
// Note that MQTT prefers dotted quad address, but MQTTS prefers fully qualified domain names (fqdn)
// Note that MQTTS works best using NTP to obtain Time
char mqttServer[64] = "";
char mqttPort[6] = "1883";
char mqttUser[32] = "";
char mqttPassword[32] = "";
char haspNode[16] = "plate01";
char groupName[16] = "plates";
char configUser[32] = "admin";
char configPassword[32] = "";
char motionPinConfig[3] = "0";
////////////////////////////////////////////////////////////////////////////////////////////////////

#define HASP_VERSION (0.40)               // Current HASP software release version
#define WIFI_CONFIG_PASSWORD ("hasplate") // First-time config WPA2 password
#define WIFI_CONFIG_AP ("HASwitchPlate")  // First-time config WPA2 password
#define CONNECTION_TIMEOUT (300)          // Timeout for WiFi and MQTT connection attempts in seconds
#define RECONNECT_TIMEOUT (15)            // Timeout for WiFi reconnection attempts in seconds

#define NEXTION_REPORT_PAGE0 (false)  // If false, don't report page 0 sendme
#define NEXTION_RETRY_MAX (5)         // Attempt to connect to panel this many times
#define NEXTION_CHECK_INTERVAL (5000) // Time in msec between nextion connection checks

#define MQTT_MAX_PACKET_SIZE (4096)  // Size of buffer for incoming MQTT message

#define DEBUG_HMI_VERBOSE (true)  // set false to have fewer printf from HMI/Nextion
#define DEBUG_MQTT_VERBOSE (true) // set false to have fewer printf from MQTT
