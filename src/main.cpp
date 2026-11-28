#include <Arduino.h>
#include "BluetoothTelemetry.h"
#include "Config.h"
#include "MotorController.h"
#include "TargetFollower.h"
#include "TargetSelector.h"
#include "UltrasonicArray.h"
#include "UltrasonicArrayTest.h"

namespace {

MotorController motor(
    Config::MotorPins::PIN_A1A,
    Config::MotorPins::PIN_A1B,
    Config::MotorPins::PIN_B1A,
    Config::MotorPins::PIN_B1B
);
UltrasonicArray ultrasonicArray;
BluetoothTelemetry bluetoothTelemetry;
TargetSelector targetSelector;
TargetFollower targetFollower(motor);
bool ultrasonicArrayReady = false;
uint32_t lastInitializationStatusMs = 0;

}

void setup() {
    Serial.begin(Config::SERIAL_BAUD_RATE);
    delay(500);
    Serial.println();
    Serial.println("============================================================");
    Serial.println("ESP32 TRACKING CAR | ULTRASONIC TARGET FOLLOWING");
    Serial.println("Mode: Moving-target tracking | Static wall filtering: ENABLED");
    Serial.println("============================================================");

    motor.init();
    motor.stop();

    if (!bluetoothTelemetry.begin()) {
        Serial.println("BLE telemetry initialization failed.");
    } else {
        Serial.printf(
            "BLE telemetry ready | Device: %s | Service: %s\n",
            Config::BluetoothParams::DEVICE_NAME,
            Config::BluetoothParams::SERVICE_UUID
        );
    }

    ultrasonicArrayReady = ultrasonicArray.init();
    if (!ultrasonicArrayReady) {
        Serial.println();
        Serial.println("ULTRASONIC ARRAY INITIALIZATION FAILED");
        Serial.printf(
            "MCP23017 I2C address: 0x%02X | SDA: GPIO %d | SCL: GPIO %d\n",
            static_cast<unsigned>(Config::UltrasonicArrayPins::MCP_I2C_ADDR),
            Config::UltrasonicArrayPins::I2C_SDA_PIN,
            Config::UltrasonicArrayPins::I2C_SCL_PIN
        );
        Serial.println("Check MCP23017 power, RESET, I2C wiring and 3.3 V pull-ups.");
        Serial.println("Motor: STOPPED | Waiting for ultrasonic array initialization.");
        return;
    }

    UltrasonicArrayTest::printStartupStatus(ultrasonicArray);
}

void loop() {
    if (!ultrasonicArrayReady) {
        motor.stop();
        const uint32_t now = millis();
        if (now - lastInitializationStatusMs >= 2000) {
            Serial.printf(
                "Still waiting for MCP23017 at I2C 0x%02X | SDA: GPIO %d | SCL: GPIO %d | Check power, RESET and I2C wiring.\n",
                static_cast<unsigned>(Config::UltrasonicArrayPins::MCP_I2C_ADDR),
                Config::UltrasonicArrayPins::I2C_SDA_PIN,
                Config::UltrasonicArrayPins::I2C_SCL_PIN
            );
            lastInitializationStatusMs = now;
        }
        delay(10);
        return;
    }

    UltrasonicArrayTest::runPollingCycle(
        ultrasonicArray,
        bluetoothTelemetry,
        targetSelector,
        targetFollower
    );
}
