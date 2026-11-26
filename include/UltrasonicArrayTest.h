#pragma once

#include <Arduino.h>
#include "Config.h"
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
    Serial.printf(
        "event=test_ready level=info mode=ultrasonic_array_polling sensor_count=%u order=F,FL,FR,LE,RI,R,RL,RR motor=stopped\n",
        static_cast<unsigned>(array.getSensorCount())
    );
}

inline void runPollingCycle(UltrasonicArray& array) {
    static uint32_t scanNumber = 0;
    ++scanNumber;
    uint8_t validCount = 0;
    const uint32_t scanStartMs = millis();

    Serial.printf(
        "event=scan_start level=info scan=%lu sensor_count=%u order=0_to_7\n",
        static_cast<unsigned long>(scanNumber),
        static_cast<unsigned>(array.getSensorCount())
    );

    for (uint8_t sensorIndex = 0;
         sensorIndex < array.getSensorCount();
         ++sensorIndex) {
        const uint8_t trigPin =
            Config::UltrasonicArrayPins::TRIG_MCP_PINS[sensorIndex];
        const uint8_t echoPin = array.getEchoPin(sensorIndex);
        const char* direction = getDirectionName(sensorIndex);

        Serial.printf(
            "event=sensor_measurement level=info state=measuring scan=%lu sensor_index=%u direction=%s trig_mcp=GPA%u echo_gpio=%u\n",
            static_cast<unsigned long>(scanNumber),
            static_cast<unsigned>(sensorIndex),
            direction,
            static_cast<unsigned>(trigPin),
            static_cast<unsigned>(echoPin)
        );

        const float distanceCm = array.measureDistance(sensorIndex);
        if (distanceCm > 0.0f) {
            ++validCount;
            Serial.printf(
                "event=sensor_measurement level=info state=valid scan=%lu sensor_index=%u direction=%s echo_gpio=%u distance_cm=%.1f\n",
                static_cast<unsigned long>(scanNumber),
                static_cast<unsigned>(sensorIndex),
                direction,
                static_cast<unsigned>(echoPin),
                distanceCm
            );
        } else {
            Serial.printf(
                "event=sensor_measurement level=warning state=no_valid_reading scan=%lu sensor_index=%u direction=%s echo_gpio=%u reason=timeout_or_out_of_range\n",
                static_cast<unsigned long>(scanNumber),
                static_cast<unsigned>(sensorIndex),
                direction,
                static_cast<unsigned>(echoPin)
            );
        }
    }

    Serial.printf(
        "event=scan_complete level=info scan=%lu valid_count=%u no_valid_count=%u elapsed_ms=%lu motor=stopped\n",
        static_cast<unsigned long>(scanNumber),
        static_cast<unsigned>(validCount),
        static_cast<unsigned>(array.getSensorCount() - validCount),
        static_cast<unsigned long>(millis() - scanStartMs)
    );
}

}
