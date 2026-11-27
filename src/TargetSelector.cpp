#include "TargetSelector.h"

#include <math.h>

namespace {

bool isValidTrackReading(const StandardizedTelemetry::Sensor& sensor) {
    return sensor.status == StandardizedTelemetry::SensorStatus::Valid &&
        sensor.index < Config::UltrasonicArrayPins::SENSOR_COUNT &&
        sensor.distanceCm >= Config::UltrasonicArrayPins::MIN_DISTANCE_CM &&
        sensor.distanceCm <= Config::TrackingParams::MAX_TRACK_DISTANCE_CM;
}

}

TargetSelector::TargetSelector() {
    for (uint8_t i = 0; i < Config::UltrasonicArrayPins::SENSOR_COUNT; ++i) {
        _history[i] = {0.0f, 0, false};
    }
}

TargetSelector::Target TargetSelector::update(
    const StandardizedTelemetry::Sensor* sensors,
    uint8_t sensorCount
) {
    if (sensors == nullptr) {
        for (uint8_t i = 0; i < Config::UltrasonicArrayPins::SENSOR_COUNT; ++i) {
            _history[i].hasPreviousReading = false;
            _history[i].motionScore = 0;
        }
        return {false, 0, 0.0f, 0.0f};
    }

    float changes[Config::UltrasonicArrayPins::SENSOR_COUNT] = {};
    bool hasChange[Config::UltrasonicArrayPins::SENSOR_COUNT] = {};

    const uint8_t count = sensorCount < Config::UltrasonicArrayPins::SENSOR_COUNT
        ? sensorCount
        : Config::UltrasonicArrayPins::SENSOR_COUNT;
    for (uint8_t i = 0; i < count; ++i) {
        const StandardizedTelemetry::Sensor& sensor = sensors[i];
        if (!isValidTrackReading(sensor)) {
            continue;
        }

        SensorHistory& history = _history[sensor.index];
        if (history.hasPreviousReading) {
            changes[sensor.index] = fabsf(sensor.distanceCm - history.distanceCm);
            hasChange[sensor.index] = true;
        }
    }

    Target nearest = {false, 0, 0.0f, 0.0f};
    for (uint8_t i = 0; i < count; ++i) {
        const StandardizedTelemetry::Sensor& sensor = sensors[i];
        if (!isValidTrackReading(sensor)) {
            continue;
        }

        SensorHistory& history = _history[sensor.index];
        if (hasChange[sensor.index] &&
            changes[sensor.index] >= Config::TrackingParams::MIN_TARGET_MOVEMENT_CM) {
            float otherChanges[Config::UltrasonicArrayPins::SENSOR_COUNT - 1] = {};
            uint8_t otherChangeCount = 0;
            for (uint8_t j = 0; j < Config::UltrasonicArrayPins::SENSOR_COUNT; ++j) {
                if (j != sensor.index && hasChange[j]) {
                    otherChanges[otherChangeCount++] = changes[j];
                }
            }

            bool isolatedMotion = otherChangeCount == 0;
            if (otherChangeCount > 0) {
                for (uint8_t j = 1; j < otherChangeCount; ++j) {
                    const float value = otherChanges[j];
                    uint8_t k = j;
                    while (k > 0 && otherChanges[k - 1] > value) {
                        otherChanges[k] = otherChanges[k - 1];
                        --k;
                    }
                    otherChanges[k] = value;
                }
                const float medianOtherChange =
                    otherChanges[otherChangeCount / 2];
                isolatedMotion =
                    changes[sensor.index] >= medianOtherChange +
                        Config::TrackingParams::MIN_TARGET_MOVEMENT_CM;
            }

            if (isolatedMotion &&
                history.motionScore <
                    Config::TrackingParams::TARGET_MOTION_SCORE_THRESHOLD) {
                ++history.motionScore;
            } else if (history.motionScore > 0) {
                --history.motionScore;
            }
        } else if (history.motionScore > 0) {
            --history.motionScore;
        }

        if (history.motionScore >=
                Config::TrackingParams::TARGET_MOTION_SCORE_THRESHOLD &&
            (!nearest.found || sensor.distanceCm < nearest.distanceCm)) {
            nearest = {
                true,
                sensor.index,
                sensor.distanceCm,
                Config::UltrasonicArrayPins::SENSOR_ANGLES[sensor.index]
            };
        }

        history.distanceCm = sensor.distanceCm;
        history.hasPreviousReading = true;
    }

    for (uint8_t i = 0; i < Config::UltrasonicArrayPins::SENSOR_COUNT; ++i) {
        bool isCurrentlyValid = false;
        for (uint8_t j = 0; j < count; ++j) {
            if (sensors[j].index == i && isValidTrackReading(sensors[j])) {
                isCurrentlyValid = true;
                break;
            }
        }
        if (!isCurrentlyValid) {
            _history[i].hasPreviousReading = false;
            if (_history[i].motionScore > 0) {
                --_history[i].motionScore;
            }
        }
    }

    return nearest;
}
