#pragma once

#include <Arduino.h>
#include "Config.h"
#include "MCP23017.h"

/**
 * ==============================================================================
 * Tracking Car - 多路超音波測距陣列 (UltrasonicArray.h)
 * ==============================================================================
 * MCP23017 依感測器索引輸出 Trig；ESP32 直接從各自的 GPIO 讀取 Echo。
 */

class UltrasonicArray {
public:
    UltrasonicArray();

    /**
     * @brief 初始化 I2C、MCP23017 Trig 輸出與各 ESP32 Echo 輸入
     * @return true 若 I2C 及 MCP23017 初始化成功，false 若通訊失敗
     */
    bool init();

    /**
     * @brief 【核心抽象方法】指定特定編號之超音波執行一次測距
     * 
     * 運作邏輯：
     * 1. 檢查 sensorIndex 是否在合法範圍內。
     * 2. 透過 MCP23017 對該感測器輸出 Trig 脈衝。
     * 3. 透過對應 ESP32 GPIO 計時 Echo 高電位脈衝。
     * 4. 換算為公分並回傳；若超時或超出上下限則返回 -1.0f。
     * 
     * @param sensorIndex 感測器編號 (0 ~ sensorCount - 1)
     * @return float 測得距離 (公分)。若無回波或超出範圍則返回 -1.0f
     */
    float measureDistance(uint8_t sensorIndex);

    /**
     * @brief 取得指定感測器之實體安裝方位角 (度，車頭前方為 0°)
     */
    float getSensorAngle(uint8_t sensorIndex) const;

    /**
     * @brief 取得目前管理的感測器總數量
     */
    uint8_t getSensorCount() const {
        return Config::UltrasonicArrayPins::SENSOR_COUNT;
    }

    /**
     * @brief 取得指定感測器對應的 ESP32 Echo GPIO
     */
    uint8_t getEchoPin(uint8_t sensorIndex) const {
        return sensorIndex < Config::UltrasonicArrayPins::SENSOR_COUNT
            ? Config::UltrasonicArrayPins::ECHO_GPIO_PINS[sensorIndex]
            : 255;
    }

private:
    MCP23017   _mcp;
    uint32_t   _timeoutUs;
    uint32_t   _lastPingTimestamp;
    bool       _hasPinged;

    void triggerSensor(uint8_t sensorIndex);
    unsigned long measureEchoDurationUs(uint8_t sensorIndex);

    static constexpr float SOUND_SPEED_FACTOR = 0.0343f / 2.0f; // 換算係數 (cm/us / 2)
};
