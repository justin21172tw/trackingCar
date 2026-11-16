#include "UltrasonicSensor.h"

UltrasonicSensor::UltrasonicSensor(uint8_t id, uint8_t trigPin, uint8_t echoPin,
                                   float angleOffset,
                                   float maxDistanceCm,
                                   float minDistanceCm)
    : _id(id), _trigPin(trigPin), _echoPin(echoPin),
      _angleOffset(angleOffset),
      _maxDistanceCm(maxDistanceCm),
      _minDistanceCm(minDistanceCm),
      _lastDistance(-1.0f),
      _lastMeasureTime(0),
      _isValid(false) {
    // 依最大距離計算 pulseIn 超時時間 (來回微秒數 + 1ms 緩衝裕度)
    // t = 2 * d / v = (2 * maxDistance) / 0.0343 + 1000
    _timeoutUs = (uint32_t)((2.0f * _maxDistanceCm / 0.0343f) + 1000.0f);
}

void UltrasonicSensor::init() {
    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);
    digitalWrite(_trigPin, LOW);
}

float UltrasonicSensor::measureDistance() {
    // 確保 Trig 處於低電位
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(2);

    // 發送 10us 高電位觸發脈衝
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);

    // 讀取 Echo 高電位持續時間 (以微秒為單位，帶超時防呆)
    unsigned long duration = pulseIn(_echoPin, HIGH, _timeoutUs);

    // duration 為 0 表示超時無回波
    if (duration == 0) {
        _lastDistance = -1.0f;
        _isValid = false;
        return -1.0f;
    }

    // 計算距離 (cm): 距離 = (時間 * 音速) / 2
    float distance = duration * SOUND_SPEED_FACTOR;

    // 檢查數值是否在合法有效範圍內
    if (distance >= _minDistanceCm && distance <= _maxDistanceCm) {
        _lastDistance = distance;
        _isValid = true;
        _lastMeasureTime = millis();
        return distance;
    } else {
        _lastDistance = -1.0f;
        _isValid = false;
        return -1.0f;
    }
}

float UltrasonicSensor::measureMedian(uint8_t samples, uint8_t delayMs) {
    if (samples == 0) return -1.0f;
    if (samples > 9) samples = 9; // 限制最大採樣數，防止陣列過大與延遲

    float values[9];
    uint8_t validCount = 0;

    for (uint8_t i = 0; i < samples; ++i) {
        float d = measureDistance();
        if (d > 0.0f) {
            values[validCount++] = d;
        }
        if (i < samples - 1 && delayMs > 0) {
            delay(delayMs);
        }
    }

    if (validCount == 0) {
        _lastDistance = -1.0f;
        _isValid = false;
        return -1.0f;
    }

    // 簡易氣泡排序取得中位數
    for (uint8_t i = 0; i < validCount - 1; ++i) {
        for (uint8_t j = 0; j < validCount - i - 1; ++j) {
            if (values[j] > values[j + 1]) {
                float tmp = values[j];
                values[j] = values[j + 1];
                values[j + 1] = tmp;
            }
        }
    }

    float medianDistance = values[validCount / 2];
    _lastDistance = medianDistance;
    _isValid = true;
    _lastMeasureTime = millis();
    return medianDistance;
}
