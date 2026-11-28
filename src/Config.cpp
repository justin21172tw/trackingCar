#include "Config.h"

constexpr uint8_t Config::UltrasonicArrayPins::TRIG_MCP_PINS[
    Config::UltrasonicArrayPins::SENSOR_COUNT
];

constexpr uint8_t Config::UltrasonicArrayPins::ECHO_GPIO_PINS[
    Config::UltrasonicArrayPins::SENSOR_COUNT
];

constexpr float Config::UltrasonicArrayPins::SENSOR_ANGLES[
    Config::UltrasonicArrayPins::SENSOR_COUNT
];

constexpr char Config::BluetoothParams::DEVICE_NAME[];
constexpr char Config::BluetoothParams::SERVICE_UUID[];
constexpr char Config::BluetoothParams::TX_CHARACTERISTIC_UUID[];
