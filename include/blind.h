#ifndef MSB_BLIND_H
#define MSB_BLIND_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <WebSerial.h>
#include "blindSensors.h"
#include "blindStatus.h"

extern String DEBUGTEXT;

class blind
{
private:
    // Blind Data
    char _mac[18];
    byte _key[6];
    char _name[20];
    int _angle;
    int _newAngle; // Used when updating blind
    bool _needUnlock;

    // UUIDs to control the blinds
    static const std::string _SERVICE_UUID;
    static const std::string _NAME_UUID;
    static const std::string _STATUS_UUID;
    static const std::string _ANGLE_UUID;
    static const std::string _KEY_UUID;
    static const std::string _SENSORS_UUID;

    BLEClient *_pClient;
    bool b_connectionError;

    void _writeAngle();
    bool connect();
    void disconnect();
    bool unlock();

public:
    blind(char *mac_addr, byte *key);
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
    bool connectionError();
    void resetError();
};



#endif