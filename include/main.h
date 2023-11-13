#include <Arduino.h>
#include <ArduinoOTA.h>
#include <arduino-timer.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <LittleFS.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// #include <ESPAutoWiFiConfig.h>
#include <WebSerial.h>
#include <ArduinoHA.h>
#include <UMS3.h>

#include "blind.h"
#include "scanner.h"
#include "config.h"
#include "Version.h"

// Emit a compiler error if hardware is not compatible
#ifndef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#error "Not compatible hardware"
#endif


#ifdef ENABLE_DEBUG
#define DEBUG_PRINT(x) WebSerial.print(x); Serial.print(x)
#define DEBUG_PRINTLN(x) WebSerial.println(x); Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif


String readFileIntoString(String filename);
bool onRefreshBLEScan( void *args);
bool onRefreshBlinds(void *args);
bool onReboot(void *args);

bool onHandleOTA( void *args );

void handle_OnConnect(AsyncWebServerRequest *request);
void handle_Command(AsyncWebServerRequest *request);
void handle_OnReturnFile(AsyncWebServerRequest *request);

String handle_OnConnectProcessor(const String &var);
String handle_OnConnectProcessor_devices(const String &var);
String handle_OnConnectProcessor_scanresults(const String &var);

void onWebSerial_recvMsg(uint8_t *data, size_t len);
blind *findBlindByMac(const char *mac);
