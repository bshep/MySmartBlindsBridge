#ifndef MSB_BLIND_H
#define MSB_BLIND_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <WebSerial.h>
#include <SafeString.h>
#include "blindSensors.h"
#include "blindStatus.h"

extern SafeString DEBUGTEXT;

class blind
{
private:
    // Blind Data
    char _mac[18];
    byte _key[6];
    char _name[20];
    int _angle;
    int _newAngle; //Used when updating blind
    bool _needUnlock;


    // UUIDs to control the blinds
    const std::string _SERVICE_UUID = "00001400-1212-efde-1600-785feabcd123";
    const std::string _NAME_UUID =    "00001401-1212-efde-1600-785feabcd123";
    const std::string _STATUS_UUID =  "00001402-1212-efde-1600-785feabcd123";
    const std::string _ANGLE_UUID =   "00001403-1212-efde-1600-785feabcd123";
    const std::string _KEY_UUID =     "00001409-1212-efde-1600-785feabcd123";
    const std::string _SENSORS_UUID = "00001651-1212-efde-1600-785feabcd123";

    BLEClient*  _pClient;

    void _writeAngle();
    bool connect();
    void disconnect();
    void unlock();
public:
    blind(const char *mac_addr, byte *key);
    ~blind();
    char *mac();
    byte *key();
    char *name();
    void refresh();
    void refreshSensors();
    void refreshStatus();
    blindSensors *sensors;
    blindStatus *status;

    void setAngle(int newAngle);
    int getAngle();
    bool isConnected();

};

#endif