#include <Ethernet.h>
#include <ArduinoHA.h>

#define INPUT_PIN       9
#define BROKER_ADDR     IPAddress(192,168,0,17)

byte mac[] = {0x00, 0x10, 0xFA, 0x6E, 0x38, 0x4A};
unsigned long lastReadAt = millis();
unsigned long lastAvailabilityToggleAt = millis();
bool lastInputState = false;

EthernetClient client;
HADevice device(mac, sizeof(mac));
HAMqtt mqtt(client, device);

// "myInput" is unique ID of the sensor. You should define you own ID.
HABinarySensor sensor("myInput");

void setup() {
    pinMode(INPUT_PIN, INPUT_PULLUP);
    lastInputState = digitalRead(INPUT_PIN);

    // you don't need to verify return status
    Ethernet.begin(mac);

    // turn on "availability" feature
    // this method also sets initial availability so you can use "true" or "false"
    sensor.setAvailability(false);

    lastReadAt = millis();
    lastAvailabilityToggleAt = millis();

    // set device's details (optional)
    device.setName("Arduino");
    device.setSoftwareVersion("1.0.0");

    sensor.setCurrentState(lastInputState); // optional
    sensor.setName("Door sensor"); // optional
    sensor.setDeviceClass("door"); // optional

    mqtt.begin(BROKER_ADDR);
}

void loop() {
    Ethernet.maintain();
    mqtt.loop();

    if ((millis() - lastReadAt) > 30) { // read in 30ms interval
        // library produces MQTT message if a new state is different than the previous one
        sensor.setState(digitalRead(INPUT_PIN));
        lastInputState = sensor.getCurrentState();
        lastReadAt = millis();
    }

    if ((millis() - lastAvailabilityToggleAt) > 5000) {
        sensor.setAvailability(!sensor.isOnline());
        lastAvailabilityToggleAt = millis();
    }
}
