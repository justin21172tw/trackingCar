#pragma once

#include <Arduino.h>

namespace StandardizedTelemetry {

static constexpr uint8_t SCHEMA_VERSION = 1;
static constexpr char LINE_PREFIX[] = "@TELEMETRY ";

enum class SensorStatus : uint8_t {
    Valid,
    Timeout,
    Invalid
};

enum class SensorReason : uint8_t {
    None,
    Timeout,
    OutOfRange,
    InvalidSensor
};

struct Sensor {
    uint8_t index;
    const char* direction;
    float distanceCm;
    SensorStatus status;
    SensorReason reason;
};

struct Logic {
    const char* state;
    bool hasTarget;
    uint8_t targetSensorIndex;
    float targetDirectionDeg;
    float targetDistanceCm;
};

struct Scan {
    uint32_t timestampMs;
    uint32_t scanNumber;
    uint32_t durationMs;
    uint8_t validCount;
    Logic logic;
    uint8_t sensorCount;
    const Sensor* sensors;
};

inline const char* statusName(SensorStatus status) {
    switch (status) {
        case SensorStatus::Valid:
            return "valid";
        case SensorStatus::Timeout:
            return "timeout";
        case SensorStatus::Invalid:
            return "invalid";
    }
    return "invalid";
}

inline const char* reasonName(SensorReason reason) {
    switch (reason) {
        case SensorReason::None:
            return "null";
        case SensorReason::Timeout:
            return "\"timeout\"";
        case SensorReason::OutOfRange:
            return "\"out_of_range\"";
        case SensorReason::InvalidSensor:
            return "\"invalid_sensor\"";
    }
    return "\"unknown\"";
}

inline void writeLine(Print& output, const Scan& scan) {
    output.print(LINE_PREFIX);
    output.print("{\"v\":");
    output.print(static_cast<unsigned>(SCHEMA_VERSION));
    output.print(",\"type\":\"scan\",\"timestamp_ms\":");
    output.print(static_cast<unsigned long>(scan.timestampMs));
    output.print(",\"scan\":");
    output.print(static_cast<unsigned long>(scan.scanNumber));
    output.print(",\"scan_duration_ms\":");
    output.print(static_cast<unsigned long>(scan.durationMs));
    output.print(",\"valid_count\":");
    output.print(static_cast<unsigned>(scan.validCount));
    output.print(",\"logic\":{\"state\":\"");
    output.print(scan.logic.state);
    output.print("\",\"target_sensor_id\":");
    if (scan.logic.hasTarget) {
        output.print("\"S");
        output.print(static_cast<unsigned>(scan.logic.targetSensorIndex));
        output.print("\"");
    } else {
        output.print("null");
    }
    output.print(",\"target_direction_deg\":");
    if (scan.logic.hasTarget) {
        output.print(scan.logic.targetDirectionDeg, 1);
    } else {
        output.print("null");
    }
    output.print(",\"target_distance_cm\":");
    if (scan.logic.hasTarget) {
        output.print(scan.logic.targetDistanceCm, 1);
    } else {
        output.print("null");
    }
    output.print('}');
    output.print(",\"sensors\":[");

    for (uint8_t i = 0; i < scan.sensorCount; ++i) {
        const Sensor& sensor = scan.sensors[i];
        if (i > 0) {
            output.print(',');
        }

        output.print("{\"id\":\"S");
        output.print(static_cast<unsigned>(sensor.index));
        output.print("\",\"direction\":\"");
        output.print(sensor.direction);
        output.print("\",\"distance_cm\":");
        if (sensor.status == SensorStatus::Valid) {
            output.print(sensor.distanceCm, 1);
        } else {
            output.print("null");
        }
        output.print(",\"status\":\"");
        output.print(statusName(sensor.status));
        output.print("\",\"reason\":");
        output.print(reasonName(sensor.reason));
        output.print('}');
    }

    output.println("]}");
}

}
