#include <Arduino.h>
#include "Config.h"
#include "MotorController.h"
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
bool ultrasonicArrayReady = false;
uint32_t lastInitializationStatusMs = 0;

}

void setup() {
    Serial.begin(Config::SERIAL_BAUD_RATE);
    delay(500);
    Serial.println();
    Serial.println("============================================================");
    Serial.println("ESP32 TRACKING CAR | ULTRASONIC ARRAY TEST");
    Serial.println("Mode: Sensor polling only | Motor action: DISABLED");
    Serial.println("============================================================");

    motor.init();
    motor.stop();

    ultrasonicArrayReady = ultrasonicArray.init();
    if (!ultrasonicArrayReady) {
        Serial.println();
        Serial.println("ULTRASONIC TEST INITIALIZATION FAILED");
        Serial.printf(
            "MCP23017 I2C address: 0x%02X | SDA: GPIO %d | SCL: GPIO %d\n",
            static_cast<unsigned>(Config::UltrasonicArrayPins::MCP_I2C_ADDR),
            Config::UltrasonicArrayPins::I2C_SDA_PIN,
            Config::UltrasonicArrayPins::I2C_SCL_PIN
        );
        Serial.println("Check MCP23017 power, RESET, I2C wiring and 3.3 V pull-ups.");
        Serial.println("Motor: STOPPED | Test will keep waiting for initialization.");
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

    UltrasonicArrayTest::runPollingCycle(ultrasonicArray);
}
