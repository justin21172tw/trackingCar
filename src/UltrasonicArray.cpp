#include "UltrasonicArray.h"

UltrasonicArray::UltrasonicArray()
    : _currentMuxChannel(-1),
      _lastPingTimestamp(0) {
    // 載入 Config.h 中之預設參數
    _config.sensorCount        = Config::UltrasonicArrayPins::SENSOR_COUNT;
    _config.mcpI2cAddr         = Config::UltrasonicArrayPins::MCP_I2C_ADDR;
    _config.sdaPin             = Config::UltrasonicArrayPins::I2C_SDA_PIN;
    _config.sclPin             = Config::UltrasonicArrayPins::I2C_SCL_PIN;
    _config.i2cClock           = Config::UltrasonicArrayPins::I2C_CLOCK;
    _config.muxControlViaMcp   = Config::UltrasonicArrayPins::MUX_CONTROL_VIA_MCP;

    _config.esp32MuxS0         = Config::UltrasonicArrayPins::ESP32_MUX_S0;
    _config.esp32MuxS1         = Config::UltrasonicArrayPins::ESP32_MUX_S1;
    _config.esp32MuxS2         = Config::UltrasonicArrayPins::ESP32_MUX_S2;
    _config.esp32MuxS3         = Config::UltrasonicArrayPins::ESP32_MUX_S3;
    _config.esp32MuxEn         = Config::UltrasonicArrayPins::ESP32_MUX_EN;

    _config.mcpMuxS0           = Config::UltrasonicArrayPins::MCP_MUX_S0;
    _config.mcpMuxS1           = Config::UltrasonicArrayPins::MCP_MUX_S1;
    _config.mcpMuxS2           = Config::UltrasonicArrayPins::MCP_MUX_S2;
    _config.mcpMuxS3           = Config::UltrasonicArrayPins::MCP_MUX_S3;
    _config.mcpMuxEn           = Config::UltrasonicArrayPins::MCP_MUX_EN;

    _config.esp32EchoPin       = Config::UltrasonicArrayPins::ESP32_ECHO_PIN;
    _config.trigPins           = Config::UltrasonicArrayPins::TRIG_MCP_PINS;
    _config.echoChannels       = Config::UltrasonicArrayPins::ECHO_MUX_CHANNELS;
    _config.sensorAngles       = Config::UltrasonicArrayPins::SENSOR_ANGLES;

    _config.minDistanceCm      = Config::UltrasonicArrayPins::MIN_DISTANCE_CM;
    _config.maxDistanceCm      = Config::UltrasonicArrayPins::MAX_DISTANCE_CM;
    _config.minPingIntervalMs  = 6; // 預設 6ms 餘波防混疊間隔

    _mcp = MCP23017(_config.mcpI2cAddr);

    // 計算 pulseIn 超時時間 (微秒)
    _timeoutUs = (uint32_t)((2.0f * _config.maxDistanceCm / 0.0343f) + 1000.0f);

    for (uint8_t i = 0; i < MAX_SENSORS; ++i) {
        _lastDistances[i] = -1.0f;
        _lastMeasureTimes[i] = 0;
        _isValidFlags[i] = false;
    }
}

UltrasonicArray::UltrasonicArray(const ConfigData& config)
    : _config(config),
      _mcp(config.mcpI2cAddr),
      _currentMuxChannel(-1),
      _lastPingTimestamp(0) {
    if (_config.sensorCount > MAX_SENSORS) {
        _config.sensorCount = MAX_SENSORS;
    }

    _timeoutUs = (uint32_t)((2.0f * _config.maxDistanceCm / 0.0343f) + 1000.0f);

    for (uint8_t i = 0; i < MAX_SENSORS; ++i) {
        _lastDistances[i] = -1.0f;
        _lastMeasureTimes[i] = 0;
        _isValidFlags[i] = false;
    }
}

bool UltrasonicArray::init() {
    // 1. 初始化 ESP32 回波接收腳位
    pinMode(_config.esp32EchoPin, INPUT);

    // 2. 初始化 CD74HC4067 控制腳 (若設定由 ESP32 GPIO 直控)
    if (!_config.muxControlViaMcp) {
        if (_config.esp32MuxS0 >= 0) pinMode(_config.esp32MuxS0, OUTPUT);
        if (_config.esp32MuxS1 >= 0) pinMode(_config.esp32MuxS1, OUTPUT);
        if (_config.esp32MuxS2 >= 0) pinMode(_config.esp32MuxS2, OUTPUT);
        if (_config.esp32MuxS3 >= 0) pinMode(_config.esp32MuxS3, OUTPUT);
        if (_config.esp32MuxEn >= 0) {
            pinMode(_config.esp32MuxEn, OUTPUT);
            digitalWrite(_config.esp32MuxEn, LOW); // 低電位使能
        }
    }

    // 3. 初始化 MCP23017 I2C 通訊
    if (!_mcp.begin(_config.sdaPin, _config.sclPin, _config.i2cClock)) {
        return false;
    }

    // 4. 配置 MCP23017 腳位模式
    // (a) 設定所有 Trig 腳位為 OUTPUT，並拉低電位
    for (uint8_t i = 0; i < _config.sensorCount; ++i) {
        uint8_t trigPin = _config.trigPins[i];
        _mcp.pinMode(trigPin, OUTPUT);
        _mcp.digitalWrite(trigPin, LOW);
    }

    // (b) 若由 MCP23017 控制 CD74HC4067 S0~S3 腳位
    if (_config.muxControlViaMcp) {
        _mcp.pinMode(_config.mcpMuxS0, OUTPUT);
        _mcp.pinMode(_config.mcpMuxS1, OUTPUT);
        _mcp.pinMode(_config.mcpMuxS2, OUTPUT);
        _mcp.pinMode(_config.mcpMuxS3, OUTPUT);
        if (_config.mcpMuxEn >= 0) {
            _mcp.pinMode(_config.mcpMuxEn, OUTPUT);
            _mcp.digitalWrite(_config.mcpMuxEn, LOW);
        }
    }

    // 5. 預設導通頻道 0
    selectMuxChannel(0);

    return true;
}

void UltrasonicArray::selectMuxChannel(uint8_t channel) {
    if (_currentMuxChannel == channel) {
        return; // 若已處於該頻道則無需重複寫入
    }

    if (_config.muxControlViaMcp) {
        // 由 MCP23017 設定 S0~S3
        _mcp.digitalWrite(_config.mcpMuxS0, (channel & 0x01) ? HIGH : LOW);
        _mcp.digitalWrite(_config.mcpMuxS1, (channel & 0x02) ? HIGH : LOW);
        _mcp.digitalWrite(_config.mcpMuxS2, (channel & 0x04) ? HIGH : LOW);
        _mcp.digitalWrite(_config.mcpMuxS3, (channel & 0x08) ? HIGH : LOW);
    } else {
        // 由 ESP32 GPIO 直控 S0~S3
        digitalWrite(_config.esp32MuxS0, (channel & 0x01) ? HIGH : LOW);
        digitalWrite(_config.esp32MuxS1, (channel & 0x02) ? HIGH : LOW);
        digitalWrite(_config.esp32MuxS2, (channel & 0x04) ? HIGH : LOW);
        digitalWrite(_config.esp32MuxS3, (channel & 0x08) ? HIGH : LOW);
    }

    _currentMuxChannel = channel;

    // CD74HC4067 類比開關切換建立時間 (< 100ns，微延遲確保訊號穩定)
    delayMicroseconds(2);
}

void UltrasonicArray::triggerSensor(uint8_t sensorIndex) {
    uint8_t mcpPin = _config.trigPins[sensorIndex];
    // 確保 Trig 先處於低電位
    _mcp.digitalWrite(mcpPin, LOW);
    delayMicroseconds(2);

    // 發射 10us 高電位觸發脈衝
    _mcp.digitalWrite(mcpPin, HIGH);
    delayMicroseconds(10);
    _mcp.digitalWrite(mcpPin, LOW);
}

unsigned long UltrasonicArray::measureEchoDurationUs(uint8_t sensorIndex) {
    // 1. 合法性檢查
    if (sensorIndex >= _config.sensorCount) {
        return 0;
    }

    // 2. 防餘波干擾保護 (確保與前一次發射間隔充足時間)
    if (_config.minPingIntervalMs > 0) {
        uint32_t now = millis();
        uint32_t elapsed = now - _lastPingTimestamp;
        if (elapsed < _config.minPingIntervalMs) {
            delay(_config.minPingIntervalMs - elapsed);
        }
    }

    // 3. 切換 CD74HC4067 複用器，將該感測器之 Echo 連通至 ESP32
    uint8_t echoCh = _config.echoChannels[sensorIndex];
    selectMuxChannel(echoCh);

    // 4. 透過 MCP23017 發送 Trig 觸發脈衝
    triggerSensor(sensorIndex);
    _lastPingTimestamp = millis();

    // 5. 透過 ESP32 硬體計時 pulseIn 擷取 Echo 高電位持續微秒數
    return pulseIn(_config.esp32EchoPin, HIGH, _timeoutUs);
}

float UltrasonicArray::measureDistance(uint8_t sensorIndex) {
    // 1. 合法性檢查
    if (sensorIndex >= _config.sensorCount) {
        return -1.0f;
    }

    // 2. 擷取原始脈衝寬度
    unsigned long duration = measureEchoDurationUs(sensorIndex);

    // 3. 判斷回波有效性並換算距離
    if (duration == 0) {
        // 超時無反射回波
        _lastDistances[sensorIndex] = -1.0f;
        _isValidFlags[sensorIndex] = false;
        return -1.0f;
    }

    float distance = duration * SOUND_SPEED_FACTOR;

    if (distance >= _config.minDistanceCm && distance <= _config.maxDistanceCm) {
        _lastDistances[sensorIndex] = distance;
        _lastMeasureTimes[sensorIndex] = millis();
        _isValidFlags[sensorIndex] = true;
        return distance;
    } else {
        // 超出量測邊界
        _lastDistances[sensorIndex] = -1.0f;
        _isValidFlags[sensorIndex] = false;
        return -1.0f;
    }
}

float UltrasonicArray::measureMedian(uint8_t sensorIndex, uint8_t samples, uint8_t delayMs) {
    if (sensorIndex >= _config.sensorCount || samples == 0) {
        return -1.0f;
    }
    if (samples > 9) samples = 9;

    float buffer[9];
    uint8_t validCount = 0;

    for (uint8_t i = 0; i < samples; ++i) {
        float d = measureDistance(sensorIndex);
        if (d > 0.0f) {
            buffer[validCount++] = d;
        }
        if (i < samples - 1 && delayMs > 0) {
            delay(delayMs);
        }
    }

    if (validCount == 0) {
        _lastDistances[sensorIndex] = -1.0f;
        _isValidFlags[sensorIndex] = false;
        return -1.0f;
    }

    // 氣泡排序
    for (uint8_t i = 0; i < validCount - 1; ++i) {
        for (uint8_t j = 0; j < validCount - i - 1; ++j) {
            if (buffer[j] > buffer[j + 1]) {
                float tmp = buffer[j];
                buffer[j] = buffer[j + 1];
                buffer[j + 1] = tmp;
            }
        }
    }

    float medianDistance = buffer[validCount / 2];
    _lastDistances[sensorIndex] = medianDistance;
    _lastMeasureTimes[sensorIndex] = millis();
    _isValidFlags[sensorIndex] = true;
    return medianDistance;
}

float UltrasonicArray::getLastDistance(uint8_t sensorIndex) const {
    if (sensorIndex >= _config.sensorCount) return -1.0f;
    return _lastDistances[sensorIndex];
}

uint32_t UltrasonicArray::getLastMeasureTime(uint8_t sensorIndex) const {
    if (sensorIndex >= _config.sensorCount) return 0;
    return _lastMeasureTimes[sensorIndex];
}

bool UltrasonicArray::isValid(uint8_t sensorIndex) const {
    if (sensorIndex >= _config.sensorCount) return false;
    return _isValidFlags[sensorIndex];
}

float UltrasonicArray::getSensorAngle(uint8_t sensorIndex) const {
    if (sensorIndex >= _config.sensorCount || _config.sensorAngles == nullptr) {
        return 0.0f;
    }
    return _config.sensorAngles[sensorIndex];
}
