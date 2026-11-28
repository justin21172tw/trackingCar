#pragma once

#include <Arduino.h>

class BLECharacteristic;
class BLEServer;

class BluetoothTelemetry : public Print {
public:
    BluetoothTelemetry();

    bool begin();
    size_t write(uint8_t value) override;

private:
    class ServerCallbacks;
    friend class ServerCallbacks;

    static constexpr size_t LINE_BUFFER_SIZE = 1024;
    static constexpr size_t NOTIFICATION_PAYLOAD_SIZE = 20;

    BLEServer* _server;
    BLECharacteristic* _characteristic;
    volatile bool _connected;
    bool _discardingLine;
    size_t _lineLength;
    char _lineBuffer[LINE_BUFFER_SIZE];

    void publishLine();
};
