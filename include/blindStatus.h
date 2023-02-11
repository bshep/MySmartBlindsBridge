#ifndef MSB_BLIND_STATUS_H
#define MSB_BLIND_STATUS_H

#include <Arduino.h>

class blindStatus
{
private:
    u_int32_t _status;
    const u_int32_t IsReversed = 1;
    const u_int32_t IsBonding = 2;
    const u_int32_t Calibrate = 4;
    const u_int32_t Identify = 8;
    const u_int32_t ResetPasskey = 8192;
    const u_int32_t ResetFlash = 16384;
    const u_int32_t Disconnect = 32768;
    const u_int32_t IsCalibrated = 65536;
    const u_int32_t HasSolar = 131072;
    const u_int32_t IsChargingSolar = 262144;
    const u_int32_t IsChargingUSB = 524288;
    const u_int32_t IsTimeValid = 1048576;
    const u_int32_t underVoltageLockout = 2097152;
    const u_int32_t isOverTemp = 4194304;
    const u_int32_t tempOverride = 8388608;
    const u_int32_t IsPasskeyValid = 2147483648;

public:
    blindStatus(){ this->_status = 0;};
    ~blindStatus(){};
    inline void updateStatus(u_int32_t status) {this->_status = status;};

    inline bool isChargingSolar() { return (this->_status & this->IsChargingSolar); }
    inline bool isChargingUSB() { return (this->_status & this->IsChargingUSB); }
    inline bool isPasskeyValid() { return (this->_status & this->IsPasskeyValid); }
};

#endif