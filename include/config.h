#ifndef MSB_CONFIG_H
#define MSB_CONFIG_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoHA.h>

#define ID_MAXLEN 30
#define HA_MAXDEVICES 20
#define HA_MACLENGTH 18

void readWifiConfig(String &wifiSSID, String &wifiPassphrase, String &wifiHostname, String &brokerAddress);
void readBlindsConfig();
String decode_base64(String input);

#endif