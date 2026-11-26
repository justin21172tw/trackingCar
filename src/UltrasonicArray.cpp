#include "UltrasonicArray.h"

UltrasonicArray::UltrasonicArray()
    : _mcp(Config::UltrasonicArrayPins::MCP_I2C_ADDR),
      _timeoutUs(0),
      _lastPingTimestamp(0),
      _hasPinged(false) {
    _timeoutUs = static_cast<uint32_t>(
        (2.0f * Config::UltrasonicArrayPins::MAX_DISTANCE_CM / 0.0343f) + 1000.0f);
}

bool UltrasonicArray::init() {
    for (uint8_t i = 0; i < Config::UltrasonicArrayPins::SENSOR_COUNT; ++i) {
        pinMode(Config::UltrasonicArrayPins::ECHO_GPIO_PINS[i], INPUT);
    }

    if (!_mcp.begin(
            Config::UltrasonicArrayPins::I2C_SDA_PIN,
            Config::UltrasonicArrayPins::I2C_SCL_PIN,
            Config::UltrasonicArrayPins::I2C_CLOCK)) {
        return false;
    }

    for (uint8_t i = 0; i < Config::UltrasonicArrayPins::SENSOR_COUNT; ++i) {
        const uint8_t trigPin = Config::UltrasonicArrayPins::TRIG_MCP_PINS[i];
        _mcp.pinMode(trigPin, OUTPUT);
        _mcp.digitalWrite(trigPin, LOW);
    }

    return true;
}

void UltrasonicArray::triggerSensor(uint8_t sensorIndex) {
    const uint8_t mcpPin = Config::UltrasonicArrayPins::TRIG_MCP_PINS[sensorIndex];
    _mcp.digitalWrite(mcpPin, LOW);
    delayMicroseconds(2);
    _mcp.pulsePin(mcpPin, 10);
}

unsigned long UltrasonicArray::measureEchoDurationUs(uint8_t sensorIndex) {
    if (sensorIndex >= Config::UltrasonicArrayPins::SENSOR_COUNT) {
        return 0;
    }

    if (_hasPinged && Config::UltrasonicArrayPins::MIN_PING_INTERVAL_MS > 0) {
        const uint32_t elapsed = millis() - _lastPingTimestamp;
        if (elapsed < Config::UltrasonicArrayPins::MIN_PING_INTERVAL_MS) {
            delay(Config::UltrasonicArrayPins::MIN_PING_INTERVAL_MS - elapsed);
        }
    }

    triggerSensor(sensorIndex);
    _lastPingTimestamp = millis();
    _hasPinged = true;

    return pulseIn(
        Config::UltrasonicArrayPins::ECHO_GPIO_PINS[sensorIndex],
        HIGH,
        _timeoutUs
    );
}

float UltrasonicArray::measureDistance(uint8_t sensorIndex) {
    if (sensorIndex >= Config::UltrasonicArrayPins::SENSOR_COUNT) {
        return -1.0f;
    }

    const unsigned long durationUs = measureEchoDurationUs(sensorIndex);
    if (durationUs == 0) {
        return -1.0f;
    }

    const float distanceCm = durationUs * SOUND_SPEED_FACTOR;
    if (distanceCm < Config::UltrasonicArrayPins::MIN_DISTANCE_CM ||
        distanceCm > Config::UltrasonicArrayPins::MAX_DISTANCE_CM) {
        return -1.0f;
    }

    return distanceCm;
}

float UltrasonicArray::getSensorAngle(uint8_t sensorIndex) const {
    if (sensorIndex >= Config::UltrasonicArrayPins::SENSOR_COUNT) {
        return 0.0f;
    }
    return Config::UltrasonicArrayPins::SENSOR_ANGLES[sensorIndex];
}
