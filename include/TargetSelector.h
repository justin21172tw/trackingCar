#pragma once

#include <Arduino.h>
#include "Config.h"
#include "StandardizedTelemetry.h"

class TargetSelector {
public:
    struct Target {
        bool found;
        uint8_t sensorIndex;
        float distanceCm;
        float directionDeg;
    };

    TargetSelector();

    Target update(
        const StandardizedTelemetry::Sensor* sensors,
        uint8_t sensorCount
    );

private:
    struct SensorHistory {
        float distanceCm;
        uint8_t motionScore;
        bool hasPreviousReading;
    };

    SensorHistory _history[Config::UltrasonicArrayPins::SENSOR_COUNT];
};
