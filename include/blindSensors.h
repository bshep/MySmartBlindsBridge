#ifndef MSB_BLIND_SENSORS_H
#define MSB_BLIND_SENSORS_H

#include <Arduino.h>

class blindSensors
{
    private:
        u_int8_t _batteryPercentage;
        u_int8_t _reserved;
        u_int16_t _batteryVoltage;
        u_int16_t _batteryCharge;
        u_int16_t _solarPanelVoltage;
        u_int16_t _interiorTemp;
        u_int16_t _batteryTemp;
        u_int16_t _rawLightValue;

    public:
        blindSensors();
        ~blindSensors();
        void parseSensorData(byte *data);

        u_int8_t getBatteryPercentage() { return _batteryPercentage; }
        u_int16_t getBatteryVoltage() { return _batteryVoltage; }
        u_int16_t getBatteryCharge() { return _batteryCharge; }
        u_int16_t getSolarPanelVoltage() { return _solarPanelVoltage; }
        u_int16_t getInteriorTemp() { return _interiorTemp; }
        u_int16_t getBatteryTemp() { return _batteryTemp; }
        u_int16_t getRawLightValue() { return _rawLightValue; }
};

#endif