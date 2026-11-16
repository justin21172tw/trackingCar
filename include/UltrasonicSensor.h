#pragma once

#include <Arduino.h>

/**
 * @file UltrasonicSensor.h
 * @brief 模組二：單一超音波測距感測器邏輯
 * 
 * 支援 HC-SR04, US-015, RCWL-1601 等常見超音波模組。
 * 內建安全 Timeout 控制，防止 10 顆超音波輪詢時阻塞主迴圈。
 * 紀錄感測器 ID、方位角度與最後測量距離，便於目標方位解算。
 */

class UltrasonicSensor {
public:
    /**
     * @brief 建構子
     * @param id 感測器編號 (0 ~ 9，用於多感測器辨識)
     * @param trigPin 觸發腳位 (Trig Pin)
     * @param echoPin 回波腳位 (Echo Pin)
     * @param angleOffset 相對於車頭的安裝角度 (以車頭為 0 度，正值偏右，負值偏左)
     * @param maxDistanceCm 最大有效偵測距離 (公分，預設 300.0)
     * @param minDistanceCm 最小有效偵測距離 (公分，預設 2.0)
     */
    UltrasonicSensor(uint8_t id, uint8_t trigPin, uint8_t echoPin,
                     float angleOffset = 0.0f,
                     float maxDistanceCm = 300.0f,
                     float minDistanceCm = 2.0f);

    /**
     * @brief 初始化 GPIO 腳位
     */
    void init();

    /**
     * @brief 執行單次距離測量 (帶 Timeout 保護)
     * @return 測得距離 (公分)，若無回波或超出有效範圍則返回 -1.0f
     */
    float measureDistance();

    /**
     * @brief 執行多次採樣並取中位數濾波 (消除環境雜訊突波)
     * @param samples 採樣次數 (建議 3 或 5 次)
     * @param delayMs 每次採樣間隔 (毫秒，預設 15ms，避免餘波干擾)
     * @return 中位數距離 (公分)，若皆無效則返回 -1.0f
     */
    float measureMedian(uint8_t samples = 3, uint8_t delayMs = 15);

    /**
     * @brief 取得最近一次測得的距離數值 (不觸發新測量)
     * @return 最近距離 (公分)，無效則為 -1.0f
     */
    float getLastDistance() const { return _lastDistance; }

    /**
     * @brief 取得最近一次成功測量的時間戳記 (millis)
     */
    uint32_t getLastMeasureTime() const { return _lastMeasureTime; }

    /**
     * @brief 判斷最近一次測距是否在有效偵測範圍內
     */
    bool isValid() const { return _isValid; }

    /**
     * @brief 取得感測器編號 ID
     */
    uint8_t getId() const { return _id; }

    /**
     * @brief 取得感測器安裝方位角度 (度)
     */
    float getAngleOffset() const { return _angleOffset; }

    /**
     * @brief 設定超時時間 (微秒)
     */
    void setTimeout(uint32_t timeoutUs) { _timeoutUs = timeoutUs; }

private:
    uint8_t  _id;
    uint8_t  _trigPin;
    uint8_t  _echoPin;
    float    _angleOffset;
    float    _maxDistanceCm;
    float    _minDistanceCm;
    uint32_t _timeoutUs;

    float    _lastDistance;
    uint32_t _lastMeasureTime;
    bool     _isValid;

    static constexpr float SOUND_SPEED_FACTOR = 0.0343f / 2.0f; // 音速換算係數 (cm/us / 2)
};
