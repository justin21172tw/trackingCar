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
    Serial.println(
        "event=system_start level=info mode=ultrasonic_array_polling motor_action=disabled"
    );

    motor.init();
    motor.stop();

    ultrasonicArrayReady = ultrasonicArray.init();
    if (!ultrasonicArrayReady) {
        Serial.printf(
            "event=test_init level=error status=failed component=mcp23017 i2c_address=0x%02X sda_gpio=%d scl_gpio=%d action=check_power_and_i2c_wiring\n",
            static_cast<unsigned>(Config::UltrasonicArrayPins::MCP_I2C_ADDR),
            Config::UltrasonicArrayPins::I2C_SDA_PIN,
            Config::UltrasonicArrayPins::I2C_SCL_PIN
        );
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
                "event=test_wait level=error state=initialization_failed component=mcp23017 i2c_address=0x%02X sda_gpio=%d scl_gpio=%d action=check_power_and_i2c_wiring\n",
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
