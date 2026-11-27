#include "TargetSelector.h"

#include "Config.h"

TargetSelector::Target TargetSelector::selectNearest(
    const StandardizedTelemetry::Sensor* sensors,
    uint8_t sensorCount
) const {
    Target nearest = {false, 0, 0.0f, 0.0f};
    if (sensors == nullptr) {
        return nearest;
    }

    for (uint8_t i = 0; i < sensorCount; ++i) {
        const StandardizedTelemetry::Sensor& sensor = sensors[i];
        if (sensor.status != StandardizedTelemetry::SensorStatus::Valid ||
            sensor.index >= Config::UltrasonicArrayPins::SENSOR_COUNT ||
            sensor.distanceCm > Config::TrackingParams::MAX_TRACK_DISTANCE_CM) {
            continue;
        }

        if (!nearest.found || sensor.distanceCm < nearest.distanceCm) {
            nearest.found = true;
            nearest.sensorIndex = sensor.index;
            nearest.distanceCm = sensor.distanceCm;
            nearest.directionDeg =
                Config::UltrasonicArrayPins::SENSOR_ANGLES[sensor.index];
        }
    }

    return nearest;
}
