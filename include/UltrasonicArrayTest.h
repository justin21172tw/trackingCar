#pragma once

#include <Arduino.h>
#include "UltrasonicArray.h"

/**
 * ==============================================================================
 * Tracking Car - 多路超音波陣列自檢與診斷測試模組 (UltrasonicArrayTest.h)
 * ==============================================================================
 * 提供給開發者在組裝 CD74HC4067 & MCP23017 硬體時：
 * 1. 驗證 I2C 通訊與 MCP23017 連線狀態
 * 2. 測試「指定單一超音波測距」功能 (自訂 Index，不用全輪詢)
 * 3. 測試任意自訂輪詢序列 (動態輪詢驗證)
 * 4. 輸出視覺化距離條與方位角資訊至 Serial Monitor
 */

namespace UltrasonicArrayTest {

/**
 * @brief 輸出陣列啟動檢測提示
 */
inline void printStartupPrompt(UltrasonicArray& array) {
    Serial.println("\n==================================================");
    Serial.println("[!] 進入【多路超音波陣列硬體診斷模式】");
    Serial.printf ("[!] 晶片架構: CD74HC4067 (Echo MUX) + MCP23017 (Trig Expander)\n");
    Serial.printf ("[!] 感測器總數: %d 顆 | I2C 位址: 0x%02X\n", 
                   array.getSensorCount(), 
                   Config::UltrasonicArrayPins::MCP_I2C_ADDR);
    Serial.printf ("[!] MUX 控制方式: %s\n", 
                   Config::UltrasonicArrayPins::MUX_CONTROL_VIA_MCP ? "MCP23017 GPB4~GPB7" : "ESP32 GPIO");
    Serial.println("[!] 即將檢測指定超音波測距功能...");
    Serial.println("==================================================\n");
}

/**
 * @brief 測試指定單一超音波進行測距並輸出詳細數據
 * @param array 超音波陣列實例
 * @param sensorIndex 欲測試之感測器編號 (0 ~ sensorCount - 1)
 */
inline void testSingleSensor(UltrasonicArray& array, uint8_t sensorIndex) {
    float rawDist    = array.measureDistance(sensorIndex);
    float medianDist = array.measureMedian(sensorIndex, 3, 10);
    float angle      = array.getSensorAngle(sensorIndex);

    char bar[25] = {0};
    bar[0] = '[';
    float displayDist = (medianDist > 0.0f) ? medianDist : rawDist;

    if (displayDist > 0.0f) {
        int barLen = (int)(displayDist / 10.0f);
        if (barLen > 20) barLen = 20;
        for (int i = 0; i < barLen; ++i) bar[i + 1] = '=';
        for (int i = barLen; i < 20; ++i) bar[i + 1] = ' ';
    } else {
        for (int i = 0; i < 20; ++i) bar[i + 1] = ' ';
    }
    bar[21] = ']';
    bar[22] = '\0';

    if (displayDist > 0.0f) {
        Serial.printf("[Sensor #%02d | %5.1f°] Raw: %5.1f cm | Median: %5.1f cm | %s\n",
                      sensorIndex, angle, rawDist, medianDist, bar);
    } else {
        Serial.printf("[Sensor #%02d | %5.1f°] Raw:   ---   | Median:   ---   | [   OUT OF RANGE   ]\n",
                      sensorIndex, angle);
    }
}

/**
 * @brief 執行自訂序列輪詢診斷 (例如先測中央 4,5 再測兩側，非死板 0~9)
 * @param array 超音波陣列實例
 * @param sequence 欲測試之感測器編號陣列
 * @param seqLength 陣列長度
 * @param cycleDelayMs 每輪序列完成後延遲時間
 */
inline void runCustomSequenceDiagnostic(UltrasonicArray& array, 
                                        const uint8_t* sequence, 
                                        uint8_t seqLength, 
                                        uint32_t cycleDelayMs = 100) {
    Serial.println("--------------------------------------------------------------------------------");
    for (uint8_t i = 0; i < seqLength; ++i) {
        uint8_t idx = sequence[i];
        testSingleSensor(array, idx);
    }
    delay(cycleDelayMs);
}

} // namespace UltrasonicArrayTest
