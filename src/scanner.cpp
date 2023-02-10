
// #define MSB_ENABLE_BLESCANNER
#ifdef MSB_ENABLE_BLESCANNER

#include <BLEScan.h>
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include <WebSerial.h>
#include <arduino-timer.h>


static std::string BlindBLEName = "SmartBlind_DFU";

int scanTime = 5; // In seconds
BLEScan *pBLEScan;
BLEScanResults foundDevices;
bool BLERefreshNow = false;

bool scanDone = false;

extern Timer<10> timer;
extern AsyncWebServer webServer;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
    }
};

bool onRefreshBLEScan(void *args)
{
    if (BLERefreshNow)
    {
        BLERefreshNow = false;
        WebSerial.println("Updating Scan Results....");
        foundDevices = pBLEScan->start(scanTime, true);
        WebSerial.println("Done!");
    }
    return true;
}

void handle_OnScan(AsyncWebServerRequest *request)
{
  WebSerial.println("HTTP Request for: " + request->url());

  request->redirect("/");

  BLERefreshNow = !BLERefreshNow;
}

void setupScan()
{
    pBLEScan = BLEDevice::getScan(); // create new scan
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
    pBLEScan->setInterval(1000);
    pBLEScan->setWindow(200); // less or equal setInterval value

    timer.every(1000, onRefreshBLEScan);
    webServer.on("/scan", handle_OnScan);
}

#endif