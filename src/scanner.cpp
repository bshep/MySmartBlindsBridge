
#define MSB_ENABLE_BLESCANNER
#ifdef MSB_ENABLE_BLESCANNER

#include "scanner.h"

static const std::string BlindBLEName = "SmartBlind_DFU";

int scanTime = 5; // In seconds
BLEScan *pBLEScan;

bool scanDone = false;

extern bool BLEScanNow;

BLEScanResults RefreshBLEScan()
{
    BLEScanResults res;
    BLEScanNow = false;
    WebSerial.println("Updating Scan Results....");
    res = pBLEScan->start(scanTime, true);
    WebSerial.println("Done!");
    return res;
}

void setupScan()
{
    pBLEScan = BLEDevice::getScan(); // create new scan
    pBLEScan->setActiveScan(true);   // active scan uses more power, but get results faster
    pBLEScan->setInterval(1000);
    pBLEScan->setWindow(200); // less or equal setInterval value
}

#endif