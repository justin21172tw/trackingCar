#include "TargetSelector.h"

namespace {

bool isValidTrackReading(const StandardizedTelemetry::Sensor& sensor) {
    return sensor.status == StandardizedTelemetry::SensorStatus::Valid &&
        sensor.index < Config::UltrasonicArrayPins::SENSOR_COUNT &&
        sensor.distanceCm <= Config::TrackingParams::MAX_TRACK_DISTANCE_CM;
}

}

TargetSelector::TargetSelector() = default;

TargetSelector::Target TargetSelector::update(
    const StandardizedTelemetry::Sensor* sensors,
    uint8_t sensorCount
) {
    if (sensors == nullptr) {
        return {false, 0, 0.0f, 0.0f};
    }

    const uint8_t count = sensorCount < Config::UltrasonicArrayPins::SENSOR_COUNT
        ? sensorCount
        : Config::UltrasonicArrayPins::SENSOR_COUNT;

    Target nearest = {false, 0, 0.0f, 0.0f};
    for (uint8_t i = 0; i < count; ++i) {
        const StandardizedTelemetry::Sensor& sensor = sensors[i];
        if (isValidTrackReading(sensor) &&
            (!nearest.found || sensor.distanceCm < nearest.distanceCm)) {
            nearest = {
                true,
                sensor.index,
                sensor.distanceCm,
                Config::UltrasonicArrayPins::SENSOR_ANGLES[sensor.index]
            };
        }
    }

    return nearest;
}
