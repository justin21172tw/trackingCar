#include "BluetoothTelemetry.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Config.h>

class BluetoothTelemetry::ServerCallbacks : public BLEServerCallbacks {
public:
    explicit ServerCallbacks(BluetoothTelemetry& telemetry)
        : _telemetry(telemetry) {
    }

    void onConnect(BLEServer*) override {
        _telemetry._connected = true;
    }

    void onDisconnect(BLEServer* server) override {
        _telemetry._connected = false;
        server->startAdvertising();
    }

private:
    BluetoothTelemetry& _telemetry;
};

BluetoothTelemetry::BluetoothTelemetry()
    : _server(nullptr),
      _characteristic(nullptr),
      _connected(false),
      _discardingLine(false),
      _lineLength(0) {
}

bool BluetoothTelemetry::begin() {
    BLEDevice::init(Config::BluetoothParams::DEVICE_NAME);
    _server = BLEDevice::createServer();
    if (_server == nullptr) {
        return false;
    }
    _server->setCallbacks(new ServerCallbacks(*this));

    BLEService* service =
        _server->createService(Config::BluetoothParams::SERVICE_UUID);
    if (service == nullptr) {
        return false;
    }

    _characteristic = service->createCharacteristic(
        Config::BluetoothParams::TX_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    if (_characteristic == nullptr) {
        return false;
    }
    _characteristic->addDescriptor(new BLE2902());
    _characteristic->setValue("Ready");
    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    if (advertising == nullptr) {
        return false;
    }
    advertising->addServiceUUID(Config::BluetoothParams::SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();
    return true;
}

size_t BluetoothTelemetry::write(uint8_t value) {
    if (_discardingLine) {
        if (value == '\n') {
            _discardingLine = false;
            _lineLength = 0;
        }
        return 1;
    }

    if (_lineLength == LINE_BUFFER_SIZE) {
        _lineLength = 0;
        _discardingLine = value != '\n';
        Serial.println("BLE telemetry line exceeded buffer; dropping line.");
        return 0;
    }

    _lineBuffer[_lineLength++] = static_cast<char>(value);
    if (value == '\n') {
        publishLine();
        _lineLength = 0;
    }
    return 1;
}

void BluetoothTelemetry::publishLine() {
    if (!_connected || _characteristic == nullptr) {
        return;
    }

    for (size_t offset = 0; offset < _lineLength;) {
        const size_t remaining = _lineLength - offset;
        const size_t chunkLength =
            remaining < NOTIFICATION_PAYLOAD_SIZE
                ? remaining
                : NOTIFICATION_PAYLOAD_SIZE;
        _characteristic->setValue(
            reinterpret_cast<uint8_t*>(_lineBuffer + offset),
            chunkLength
        );
        _characteristic->notify();
        offset += chunkLength;
        delay(3);
    }
}
