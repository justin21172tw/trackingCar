#pragma once

#include <Arduino.h>
#include "Config.h"
#include "StandardizedTelemetry.h"
#include "TargetFollower.h"
#include "TargetSelector.h"
#include "UltrasonicArray.h"

namespace UltrasonicArrayTest {

inline const char* getDirectionName(uint8_t sensorIndex) {
    static const char* const directions[] = {
        "F", "FL", "FR", "LE", "RI", "R", "RL", "RR"
    };

    if (sensorIndex >= Config::UltrasonicArrayPins::SENSOR_COUNT) {
        return "UNKNOWN";
    }
    return directions[sensorIndex];
}

inline void printStartupStatus(const UltrasonicArray& array) {
    Serial.println();
    Serial.println("============================================================");
    Serial.println("ULTRASONIC TARGET FOLLOWER: READY");
    Serial.printf(
        "Sensors: %u | Poll order: F -> FL -> FR -> LE -> RI -> R -> RL -> RR\n",
        static_cast<unsigned>(array.getSensorCount())
    );
    Serial.printf(
        "MCP23017: I2C 0x%02X | SDA: GPIO %d | SCL: GPIO %d\n",
        static_cast<unsigned>(Config::UltrasonicArrayPins::MCP_I2C_ADDR),
        Config::UltrasonicArrayPins::I2C_SDA_PIN,
        Config::UltrasonicArrayPins::I2C_SCL_PIN
    );
    Serial.printf(
        "Minimum trigger interval: %u ms | Motor: TARGET FOLLOWING ENABLED | Static return filtering: ENABLED\n",
        static_cast<unsigned>(
            Config::UltrasonicArrayPins::MIN_PING_INTERVAL_MS
        )
    );
    Serial.println("============================================================");
}

inline void runPollingCycle(
    UltrasonicArray& array,
    TargetSelector& targetSelector,
    TargetFollower& targetFollower
) {
    static uint32_t scanNumber = 0;
    ++scanNumber;
    uint8_t validCount = 0;
    const uint32_t scanStartMs = millis();
    StandardizedTelemetry::Sensor telemetrySensors[
        Config::UltrasonicArrayPins::SENSOR_COUNT
    ];

    Serial.printf(
        "\n--- Scan #%lu started at %lu ms | Order: F -> FL -> FR -> LE -> RI -> R -> RL -> RR ---\n",
        static_cast<unsigned long>(scanNumber),
        static_cast<unsigned long>(scanStartMs)
    );

    for (uint8_t sensorIndex = 0;
         sensorIndex < array.getSensorCount();
         ++sensorIndex) {
        const uint8_t trigPin =
            Config::UltrasonicArrayPins::TRIG_MCP_PINS[sensorIndex];
        const uint8_t echoPin = array.getEchoPin(sensorIndex);
        const char* direction = getDirectionName(sensorIndex);

        Serial.printf(
            "[%u/%u] %s: Measuring...\n",
            static_cast<unsigned>(sensorIndex + 1),
            static_cast<unsigned>(array.getSensorCount()),
            direction
        );

        const UltrasonicArray::MeasurementResult measurement =
            array.measureDetailed(sensorIndex);
        if (measurement.isValid()) {
            ++validCount;
            Serial.printf(
                "[%u/%u] %s | Trig: MCP23017 GPA%u (pin %u) | Echo: ESP32 GPIO %u | Status: VALID | Distance: %.1f cm\n",
                static_cast<unsigned>(sensorIndex + 1),
                static_cast<unsigned>(array.getSensorCount()),
                direction,
                static_cast<unsigned>(trigPin),
                static_cast<unsigned>(trigPin),
                static_cast<unsigned>(echoPin),
                measurement.distanceCm
            );
        } else {
            Serial.printf(
                "[%u/%u] %s | Trig: MCP23017 GPA%u (pin %u) | Echo: ESP32 GPIO %u | Status: NO VALID READING | Reason: timeout or out of range\n",
                static_cast<unsigned>(sensorIndex + 1),
                static_cast<unsigned>(array.getSensorCount()),
                direction,
                static_cast<unsigned>(trigPin),
                static_cast<unsigned>(trigPin),
                static_cast<unsigned>(echoPin)
            );
        }

        StandardizedTelemetry::SensorStatus telemetryStatus =
            StandardizedTelemetry::SensorStatus::Invalid;
        StandardizedTelemetry::SensorReason telemetryReason =
            StandardizedTelemetry::SensorReason::OutOfRange;

        switch (measurement.status) {
            case UltrasonicArray::MeasurementStatus::Valid:
                telemetryStatus = StandardizedTelemetry::SensorStatus::Valid;
                telemetryReason = StandardizedTelemetry::SensorReason::None;
                break;
            case UltrasonicArray::MeasurementStatus::Timeout:
                telemetryStatus = StandardizedTelemetry::SensorStatus::Timeout;
                telemetryReason = StandardizedTelemetry::SensorReason::Timeout;
                break;
            case UltrasonicArray::MeasurementStatus::InvalidRange:
                telemetryReason = StandardizedTelemetry::SensorReason::OutOfRange;
                break;
            case UltrasonicArray::MeasurementStatus::InvalidSensor:
                telemetryReason = StandardizedTelemetry::SensorReason::InvalidSensor;
                break;
        }

        telemetrySensors[sensorIndex] = {
            sensorIndex,
            direction,
            measurement.distanceCm,
            telemetryStatus,
            telemetryReason
        };
    }

    const uint32_t scanDurationMs = millis() - scanStartMs;
    const TargetSelector::Target target =
        targetSelector.update(telemetrySensors, array.getSensorCount());
    const TargetFollower::State followerState = targetFollower.update(target);

    Serial.printf(
        "--- Scan #%lu complete | Valid: %u/%u | No valid reading: %u/%u | Duration: %lu ms | Follow state: %s",
        static_cast<unsigned long>(scanNumber),
        static_cast<unsigned>(validCount),
        static_cast<unsigned>(array.getSensorCount()),
        static_cast<unsigned>(array.getSensorCount() - validCount),
        static_cast<unsigned>(array.getSensorCount()),
        static_cast<unsigned long>(scanDurationMs),
        TargetFollower::stateName(followerState)
    );
    if (target.found) {
        Serial.printf(
            " | Target: S%u at %.1f cm, %.1f deg",
            static_cast<unsigned>(target.sensorIndex),
            target.distanceCm,
            target.directionDeg
        );
    }
    Serial.println(" ---");

    const StandardizedTelemetry::Scan telemetryScan = {
        millis(),
        scanNumber,
        scanDurationMs,
        validCount,
        {
            TargetFollower::stateName(followerState),
            target.found,
            target.sensorIndex,
            target.directionDeg,
            target.distanceCm
        },
        array.getSensorCount(),
        telemetrySensors
    };
    StandardizedTelemetry::writeLine(Serial, telemetryScan);
}

}
