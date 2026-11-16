#pragma once

#include <Arduino.h>
#include "Config.h"
#include "MCP23017.h"

/**
 * ==============================================================================
 * Tracking Car - 多路超音波測距陣列抽象化模組 (UltrasonicArray.h)
 * ==============================================================================
 * 
 * 硬體架構：
 * 1. CD74HC4067 (16:1 類比/數位多路複用器)：
 *    - 切換 10 顆超音波之 Echo 回波訊號，匯流至 ESP32 單一 Echo 輸入腳 (如 GPIO 34)。
 *    - 具備奈秒級低延遲通道切換，保證 microsecond pulseIn 測距精確度。
 *    - 頻道位址 S0~S3 可由 ESP32 GPIO 直控，或由 MCP23017 腳位控制 (節省腳位)。
 * 
 * 2. MCP23017 (16-Bit I2C I/O 擴充晶片)：
 *    - 控制 10 顆超音波之 Trig 發射觸發脈衝 (GPA0~GPA7, GPB0~GPB1)。
 *    - (可選) 兼顧 CD74HC4067 之 S0~S3 位址輸出，全系統僅佔用 ESP32 3 根 GPIO。
 * 
 * 設計核心：
 * - 徹底抽象化「指定特定超音波進行一次測距」的底層時序與多工邏輯。
 * - 支援非固定順序之動態輪詢 (如優先掃描目標周遭感測器)。
 * - 內建超時防卡死保護、中位數防突波濾波器、方位角度快取及防干擾餘波保護。
 */

class UltrasonicArray {
public:
    static constexpr uint8_t MAX_SENSORS = 16;

    /**
     * @brief 多路超音波組態結構
     */
    struct ConfigData {
        uint8_t sensorCount;
        uint8_t mcpI2cAddr;
        int8_t  sdaPin;
        int8_t  sclPin;
        uint32_t i2cClock;

        bool    muxControlViaMcp;
        // ESP32 直控腳位 (當 muxControlViaMcp == false)
        int8_t  esp32MuxS0;
        int8_t  esp32MuxS1;
        int8_t  esp32MuxS2;
        int8_t  esp32MuxS3;
        int8_t  esp32MuxEn;

        // MCP23017 控制腳位 (當 muxControlViaMcp == true)
        uint8_t mcpMuxS0;
        uint8_t mcpMuxS1;
        uint8_t mcpMuxS2;
        uint8_t mcpMuxS3;
        int8_t  mcpMuxEn;

        // ESP32 回波接收腳位
        uint8_t esp32EchoPin;

        // 感測器腳位與頻道映射指標 (長度須為 sensorCount)
        const uint8_t* trigPins;
        const uint8_t* echoChannels;
        const float*   sensorAngles;

        float minDistanceCm;
        float maxDistanceCm;
        uint32_t minPingIntervalMs; // 兩次 Ping 之間的最小間隔保護 (防餘波干擾，建議 5~10ms)
    };

    /**
     * @brief 預設建構子 (自動採用 Config.h 中之全域設定)
     */
    UltrasonicArray();

    /**
     * @brief 自訂組態建構子
     * @param config 自訂多路超音波設定
     */
    explicit UltrasonicArray(const ConfigData& config);

    /**
     * @brief 初始化硬體通訊、GPIO 模式、MCP23017 與 CD74HC4067 控制腳
     * @return true 若 I2C 及 MCP23017 初始化成功，false 若通訊失敗
     */
    bool init();

    /**
     * @brief 【核心抽象方法】指定特定編號之超音波執行一次測距
     * 
     * 運作邏輯：
     * 1. 檢查 sensorIndex 是否在合法範圍內。
     * 2. 控制 S0~S3 將 CD74HC4067 切換至該感測器的 Echo 頻道。
     * 3. 透過 MCP23017 發送 >= 10us 之 Trig 高電位觸發脈衝。
     * 4. 透過 ESP32 硬體計時 pulseIn 擷取 Echo 脈衝寬度 (帶超時防呆)。
     * 5. 換算為公分並寫入快取，若超時或超出上下限返回 -1.0f。
     * 
     * @param sensorIndex 感測器編號 (0 ~ sensorCount - 1)
     * @return float 測得距離 (公分)。若無回波或超出範圍則返回 -1.0f
     */
    float measureDistance(uint8_t sensorIndex);

    /**
     * @brief 指定特定超音波進行多次採樣中位數濾波測距 (過濾環境雜訊)
     * @param sensorIndex 感測器編號 (0 ~ sensorCount - 1)
     * @param samples 採樣次數 (建議 3 或 5)
     * @param delayMs 採樣間距 (毫秒，建議 10~15ms 避免餘波混疊)
     * @return float 中位數距離 (公分)，若無有效數據返回 -1.0f
     */
    float measureMedian(uint8_t sensorIndex, uint8_t samples = 3, uint8_t delayMs = 12);

    /**
     * @brief 切換 CD74HC4067 多路複用器至指定頻道 (0 ~ 15)
     * @param channel 欲導通的頻道編號
     */
    void selectMuxChannel(uint8_t channel);

    /**
     * @brief 發送 Trig 觸發脈衝給指定感測器
     * @param sensorIndex 感測器編號
     */
    void triggerSensor(uint8_t sensorIndex);

    /**
     * @brief 取得指定感測器最近一次測距數值 (不觸發新測量)
     */
    float getLastDistance(uint8_t sensorIndex) const;

    /**
     * @brief 取得指定感測器最近一次有效測量的時間戳記 (millis)
     */
    uint32_t getLastMeasureTime(uint8_t sensorIndex) const;

    /**
     * @brief 檢查指定感測器最近一次測距是否為有效數據
     */
    bool isValid(uint8_t sensorIndex) const;

    /**
     * @brief 取得指定感測器之實體安裝方位角 (度，車頭前方為 0°)
     */
    float getSensorAngle(uint8_t sensorIndex) const;

    /**
     * @brief 取得目前管理的感測器總數量
     */
    uint8_t getSensorCount() const { return _config.sensorCount; }

    /**
     * @brief 取得底層 MCP23017 實例引用 (供進階除錯或直接存取)
     */
    MCP23017& getMCP() { return _mcp; }

    /**
     * @brief 設定測距超時微秒數 (若欲動態調整)
     */
    void setTimeoutUs(uint32_t timeoutUs) { _timeoutUs = timeoutUs; }

private:
    ConfigData _config;
    MCP23017   _mcp;
    uint32_t   _timeoutUs;
    int16_t    _currentMuxChannel;
    uint32_t   _lastPingTimestamp;

    // 快取狀態陣列 (靜態配置，避免 Heap 碎片化)
    float    _lastDistances[MAX_SENSORS];
    uint32_t _lastMeasureTimes[MAX_SENSORS];
    bool     _isValidFlags[MAX_SENSORS];

    static constexpr float SOUND_SPEED_FACTOR = 0.0343f / 2.0f; // 換算係數 (cm/us / 2)
};
