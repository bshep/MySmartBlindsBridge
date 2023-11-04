#include "blind.h"
#include "scanner.h"

#define ENABLE_DEBUG

// Uncomment the following line for debugging purposes so we dont get stuck waiting on BLE comms
// #define DISABLE_COMMS

#ifdef ENABLE_DEBUG
#define DEBUG_PRINT(x)  \
    WebSerial.print(x); \
    Serial.print(x)
#define DEBUG_PRINTLN(x)  \
    WebSerial.println(x); \
    Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

const std::string blind::_SERVICE_UUID = "00001400-1212-efde-1600-785feabcd123";
const std::string blind::_NAME_UUID = "00001401-1212-efde-1600-785feabcd123";
const std::string blind::_STATUS_UUID = "00001402-1212-efde-1600-785feabcd123";
const std::string blind::_ANGLE_UUID = "00001403-1212-efde-1600-785feabcd123";
const std::string blind::_KEY_UUID = "00001409-1212-efde-1600-785feabcd123";
const std::string blind::_SENSORS_UUID = "00001651-1212-efde-1600-785feabcd123";

extern scanner *myBLEScanner;

bool isScanned(BLEAddress targetAddr)
{
    DEBUG_PRINT("isScanned(): Checking address");
    DEBUG_PRINTLN(targetAddr.toString().c_str());
    for (int a = 0; a < myBLEScanner->foundDevices.getCount(); a++)
    {
        DEBUG_PRINT(" - checking against address: ");
        DEBUG_PRINT(myBLEScanner->foundDevices.getDevice(a).getAddress().toString().c_str());
        if (myBLEScanner->foundDevices.getDevice(a).getAddress() == targetAddr)
        {
            DEBUG_PRINTLN(" - MATCH");
            return true;
        }
        DEBUG_PRINTLN(" - NO MATCH");
    }

    DEBUG_PRINTLN(" - NO MATCHES FOUND IN SCANNED DEVICES");

    return false;
}

blind::blind(char *mac_addr, byte *key)
{
    strncpy(this->_mac, mac_addr, 18);
    this->_pClient = NULL;
    this->_newAngle = 255;
    this->_angle = 254;
    strcpy(this->_name, "UNKNOWN");
    this->_needUnlock = true;
    this->sensors = new blindSensors();
    this->status = new blindStatus();
    this->b_connectionError = false;

    for (int i = 0; i < 6; i++)
    {
        this->_key[i] = key[i];
    }
}

blind::~blind()
{
}

bool blind::connect()
{
    DEBUG_PRINTLN("blind::connect() - Entered Function");

#ifdef DISABLE_COMMS
#warning "COMMS DISABLED BLE WILL NOT WORK!!!"
    return false;
#endif

    if (this->_pClient == NULL)
    {
        this->_pClient = BLEDevice::createClient();
    }

    if (this->_pClient->isConnected() == false && this->b_connectionError == false)
    {
        DEBUG_PRINT("blind::connect() - attempting to connect to: mac - ");
        DEBUG_PRINTLN(this->_mac);
        BLEAddress myAddr = BLEAddress(std::string(this->_mac));

        if (isScanned(myAddr))
        {
            this->_pClient->connect(myAddr, BLE_ADDR_TYPE_RANDOM);
            if (this->_pClient->isConnected() == false)
            {
                this->b_connectionError = true;
                return false;
            }
            this->_needUnlock = true;
        }
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

bool blind::connectionError()
{
    return this->b_connectionError;
}

void blind::resetError()
{
    this->b_connectionError = false;
}

void blind::setAngle(int newAngle)
{
    this->_newAngle = newAngle;
}

int blind::getAngle()
{
    return this->_angle;
}

bool blind::unlock()
{
    if (!this->_needUnlock)
    {
        DEBUG_PRINTLN("blind::unlock() - already unlocked");
        return true;
    }

    if (!this->connect())
    {
        DEBUG_PRINTLN("blind::unlock() - Error - Unable to connect");
        return false;
    }

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID);
    if (tmpService == NULL)
    {
        DEBUG_PRINTLN("blind::unlock() - Could not get service");
        return false;
    }

    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_KEY_UUID);

    if (tmpCharact == NULL)
    {
        DEBUG_PRINTLN("blind::unlock() - Could not get charecteristic");
        return false;
    }

    tmpCharact->writeValue(this->_key, 6, true);
    this->_needUnlock = false;
    return true;
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
    if (this->isConnected())
    {
        return this->_name;
    }
    else
        return "DISCONNECTED";
}

void blind::refresh()
{
    DEBUG_PRINTLN("blind::refresh() - Entered Function");

#ifdef DISABLE_COMMS
    return;
#endif

    if (this->unlock() == false)
    {
        DEBUG_PRINTLN("blind::refresh(): unable to unlock");
        return;
    }

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
    DEBUG_PRINTLN("ANGLE = " + String(this->_angle));
    DEBUG_PRINTLN(" -- oldAngle = " + String(this->_angle) + " - newAngle - " + String(this->_newAngle));

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
    DEBUG_PRINTLN(" -- oldAngle = " + String(this->_angle) + " - newAngle - " + String(this->_newAngle));

#ifdef DISABLE_COMMS
    return;
#endif

    if (this->_newAngle == 255)
    {
        this->_newAngle = this->_angle;
        DEBUG_PRINTLN("blind::_writeAngle() - First refresh - NOT SETTING ANGLE");

        return;
    }

    if (this->unlock())
    {
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
    }
    else
    {
        DEBUG_PRINTLN("blind::_writeAngle() - Failed Unlock");
    }
    DEBUG_PRINTLN("blind::_writeAngle() - Exit Function");
}

void blind::refreshSensors()
{

#ifdef DISABLE_COMMS
    return;
#endif
    if (this->unlock())
    {

        std::string sensorValue = this->_pClient->getValue(this->_SERVICE_UUID, this->_SENSORS_UUID);

        this->sensors->parseSensorData((byte *)sensorValue.c_str());

        DEBUG_PRINTLN("Sensors: ");
        DEBUG_PRINTLN(" - batteryPercentage=" + String(this->sensors->getBatteryPercentage()));
        DEBUG_PRINTLN(" - _batteryVoltage=" + String(this->sensors->getBatteryVoltage()));
        DEBUG_PRINTLN(" - _batteryCharge=" + String(this->sensors->getBatteryCharge()));
        DEBUG_PRINTLN(" - _solarPanelVoltage=" + String(this->sensors->getSolarPanelVoltage()));
        DEBUG_PRINTLN(" - _interiorTemp=" + String(this->sensors->getInteriorTemp()));
        DEBUG_PRINTLN(" - _batteryTemp=" + String(this->sensors->getBatteryTemp()));
        DEBUG_PRINTLN(" - _rawLightValue=" + String(this->sensors->getRawLightValue()));
    }
    else
    {
        DEBUG_PRINTLN("blind::refreshSensors() - Failed Unlock");
    }
}

void blind::refreshStatus()
{
#ifdef DISABLE_COMMS
    return;
#endif

    if (this->unlock())
    {

        std::string statusValue = this->_pClient->getValue(this->_SERVICE_UUID, this->_STATUS_UUID);

        const char *statValueChar = statusValue.c_str();

        u_int32_t statusValueLong = (statValueChar[3] << 24) + (statValueChar[2] << 16) + (statValueChar[1] << 8) + statValueChar[0];
        this->status->updateStatus(statusValueLong);

        DEBUG_PRINTLN("StatusValue: " + String(statusValueLong, 16));
    }
    else
    {
        DEBUG_PRINTLN("blind::refreshStatus() - Failed Unlock");
    }
}