#include "blind.h"

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define DEBUG_PRINT(x) WebSerial.print(x)
#define DEBUG_PRINTLN(x) WebSerial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

blind::blind(const char *mac_addr, byte *key)
{
    cSFA(sfMac, this->_mac);
    sfMac = mac_addr;
    cSFA(sfName, this->_name);
    sfName = "UNKNOWN";
    this->_pClient = NULL;
    this->_newAngle = 255;
    this->_angle = 254;
    this->_needUnlock = true;
    this->sensors = new blindSensors();
    this->status = new blindStatus();

    memcpy(this->_key, key, 6);
}

blind::~blind()
{
}

bool blind::connect()
{
    DEBUG_PRINTLN("blind::connect() - Entered Function");

    if (this->_pClient == NULL)
    {
        this->_pClient = BLEDevice::createClient();
    }

    if (this->_pClient->isConnected() == false)
    {
        DEBUG_PRINT("blind::connect() - attempting to connect to: mac - ");
        DEBUG_PRINTLN(this->_mac);
        BLEAddress myAddr = BLEAddress(std::string(this->_mac));

        this->_pClient->connect(myAddr, BLE_ADDR_TYPE_RANDOM);
        this->_needUnlock = true;
    }
    DEBUG_PRINTLN("blind::connect() - Exit Function");

    return this->_pClient->isConnected();
}

void blind::disconnect()
{
    this->_pClient->disconnect();
    this->_pClient = NULL;
}

bool blind::isConnected()
{
    return this->_pClient->isConnected();
}

void blind::setAngle(int newAngle)
{
    this->_newAngle = newAngle;
}

int blind::getAngle()
{
    return this->_angle;
}

void blind::unlock()
{
    if (!this->_needUnlock)
    {
        DEBUG_PRINTLN("blind::unlock() - already unlocked");
        return;
    }

    if (!this->connect())
    {
        DEBUG_PRINTLN("blind::unlock() - Error - Unable to connect");
    }

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID);
    if (tmpService == NULL)
    {
        DEBUG_PRINTLN("blind::unlock() - Could not get service");
    }

    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_KEY_UUID);

    if (tmpCharact == NULL)
    {
        DEBUG_PRINTLN("blind::unlock() - Could not get charecteristic");
    }

    tmpCharact->writeValue(this->_key, 6, true);
    this->_needUnlock = false;
}

char *blind::mac()
{
    return this->_mac;
}

byte *blind::key()
{
    return this->_key;
}

char *blind::name()
{
    return this->_name;
}

void blind::refresh()
{
    DEBUG_PRINTLN("blind::refresh() - Entered Function");

    this->unlock();

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID);
    if (tmpService == NULL)
    {
        DEBUG_PRINTLN("blind::refresh() - Could not get service");
        return;
    }

    // Refresh the Name of the Blind
    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_NAME_UUID);
    if (tmpCharact == NULL)
    {
        DEBUG_PRINTLN("blind::refresh() - Could not get charecteristic for NAME");
        return;
    }

    std::string tmpName = tmpCharact->readValue();

    strcpy(this->_name, tmpName.c_str() + 4);
    DEBUG_PRINT("NAME = ");
    DEBUG_PRINTLN(this->_name);

    // Refresh the Angle of the Blind
    tmpCharact = tmpService->getCharacteristic(this->_ANGLE_UUID);
    if (tmpCharact == NULL)
    {
        DEBUG_PRINTLN("blind::refresh() - Could not get charecteristic for ANGLE");
        return;
    }

    this->_angle = tmpCharact->readUInt8();
    DEBUG_PRINTLN("ANGLE = " + this->_angle);
    DEBUG_PRINT(" -- oldAngle = " + this->_angle);
    DEBUG_PRINTLN(" - newAngle - " + this->_newAngle);

    if (this->_angle != this->_newAngle)
    {
        // Need to write newAngle to blind
        this->_writeAngle();
    }

    DEBUG_PRINTLN("blind::refresh() - Exit Function");
}

void blind::_writeAngle()
{
    DEBUG_PRINTLN("blind::_writeAngle() - Entered Function");
    DEBUG_PRINT(" -- oldAngle = " + this->_angle);
    DEBUG_PRINTLN(" - newAngle - " + this->_newAngle);

    if (this->_newAngle == 255)
    {
        this->_newAngle = this->_angle;
        DEBUG_PRINTLN("blind::_writeAngle() - First refresh - NOT SETTING ANGLE");

        return;
    }

    this->unlock();

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID);
    if (tmpService == NULL)
    {
        DEBUG_PRINTLN("blind::_writeAngle() - Could not get service");
    }

    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_ANGLE_UUID);
    if (tmpCharact == NULL)
    {
        DEBUG_PRINTLN("blind::_writeAngle() - Could not get charecteristic for ANGLE");
    }
    tmpCharact->writeValue((uint8_t)this->_newAngle);
    this->_angle = _newAngle;

    DEBUG_PRINTLN("blind::_writeAngle() - Exit Function");
}

void blind::refreshSensors()
{
    this->unlock();

    std::string sensorValue = this->_pClient->getValue(this->_SERVICE_UUID, this->_SENSORS_UUID);

    this->sensors->parseSensorData((byte *)sensorValue.c_str());

    DEBUG_PRINTLN("Sensors: ");
    DEBUG_PRINTLN(" - batteryPercentage=" + this->sensors->getBatteryPercentage());
    DEBUG_PRINTLN(" - _batteryVoltage=" + this->sensors->getBatteryVoltage());
    DEBUG_PRINTLN(" - _batteryCharge=" + this->sensors->getBatteryCharge());
    DEBUG_PRINTLN(" - _solarPanelVoltage=" + this->sensors->getSolarPanelVoltage());
    DEBUG_PRINTLN(" - _interiorTemp=" + this->sensors->getInteriorTemp());
    DEBUG_PRINTLN(" - _batteryTemp=" + this->sensors->getBatteryTemp());
    DEBUG_PRINTLN(" - _rawLightValue=" + this->sensors->getRawLightValue());
}

void blind::refreshStatus()
{
    u_int32_t statusValueLong;
    this->unlock();

    std::string statusValue = this->_pClient->getValue(this->_SERVICE_UUID, this->_STATUS_UUID);

    const char *statValueChar = statusValue.c_str();
    statusValueLong = (statValueChar[3] << 24) + (statValueChar[2] << 16) + (statValueChar[1] << 8) + statValueChar[0];
    this->status->updateStatus(statusValueLong);

    DEBUG_PRINTLN("StatusValue: " + String(statusValueLong,16));
}