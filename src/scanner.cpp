
#define MSB_ENABLE_BLESCANNER
#ifdef MSB_ENABLE_BLESCANNER

#include "scanner.h"

static const std::string BlindBLEName = "SmartBlind_DFU";

scanner::scanner() {
    this->pBLEScan = BLEDevice::getScan();
    this->pBLEScan->setActiveScan(true);   // active scan uses more power, but get results faster
    this->pBLEScan->setInterval(1000);
    this->pBLEScan->setWindow(200); // less or equal setInterval value
}

scanner::~scanner() {
    this->pBLEScan->stop();
    this->pBLEScan->stopExtScan();
    this->pBLEScan->clearResults();
}

void scanner::RefreshBLEScan()
{
    WebSerial.println("Updating Scan Results....");
    this->foundDevices = pBLEScan->start(this->scanTime, true);
    WebSerial.println("Done!");
}

void scanner::setupScan()
{
    pBLEScan = BLEDevice::getScan(); // create new scan
    pBLEScan->setActiveScan(true);   // active scan uses more power, but get results faster
    pBLEScan->setInterval(1000);
    pBLEScan->setWindow(200); // less or equal setInterval value
}

#endif