#include "blindSensors.h"

blindSensors::blindSensors()
{
    _batteryPercentage = 0;
    _reserved = 0;
    _batteryVoltage = 0;
    _batteryCharge = 0;
    _solarPanelVoltage = 0;
    _interiorTemp = 0;
    _batteryTemp = 0;
    _rawLightValue = 0;
}

blindSensors::~blindSensors()
{
}

void blindSensors::parseSensorData(byte *data)
{
    u_int8_t index = 0;
    _batteryPercentage = data[index];
    index++;
    _reserved = data[index];
    index++;
    _batteryVoltage = data[index];
    index += 2;
    _batteryCharge = data[index];
    index += 2;
    _solarPanelVoltage = data[index];
    index += 2;
    _interiorTemp = data[index];
    index += 2;
    _batteryTemp = data[index];
    index += 2;
    _rawLightValue = data[index];
}