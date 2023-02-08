#ifndef MSB_CONFIG_H
#define MSB_CONFIG_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoHA.h>



void readWIFIConfig();
void readBlindsConfig();
String decode_base64(String input);

#endif