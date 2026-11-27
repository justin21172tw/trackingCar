#pragma once

#include <Arduino.h>
#include "StandardizedTelemetry.h"

class TargetSelector {
public:
    struct Target {
        bool found;
        uint8_t sensorIndex;
        float distanceCm;
        float directionDeg;
    };

    Target selectNearest(
        const StandardizedTelemetry::Sensor* sensors,
        uint8_t sensorCount
    ) const;
};
