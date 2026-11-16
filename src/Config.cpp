#include "Config.h"

constexpr uint8_t Config::UltrasonicArrayPins::TRIG_MCP_PINS[
    Config::UltrasonicArrayPins::SENSOR_COUNT
];

constexpr uint8_t Config::UltrasonicArrayPins::ECHO_MUX_CHANNELS[
    Config::UltrasonicArrayPins::SENSOR_COUNT
];

constexpr float Config::UltrasonicArrayPins::SENSOR_ANGLES[
    Config::UltrasonicArrayPins::SENSOR_COUNT
];
