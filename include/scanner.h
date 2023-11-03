#ifndef SCANNER_H
#define SCANNER_H


#include <BLEScan.h>
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include <WebSerial.h>
#include <arduino-timer.h>

BLEScanResults RefreshBLEScan(void);
void setupScan();


#endif