#ifndef SCANNER_H
#define SCANNER_H


#include <BLEScan.h>
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include <WebSerial.h>
#include <arduino-timer.h>


class scanner {
    private:
        int scanTime = 5; // In seconds
        BLEScan *pBLEScan; // The scan instance

bool scanDone = false;
    public:
        scanner(void);
        ~scanner();
        void RefreshBLEScan(void);
        void setupScan();
        
        BLEScanResults foundDevices;
};


#endif