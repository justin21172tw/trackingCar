#pragma once

#include <Arduino.h>
#include "UltrasonicSensor.h"

/**
 * ==============================================================================
 * Tracking Car - 單一超音波感測器診斷與測試模組 (UltrasonicTest.h)
 * ==============================================================================
 * 本模組提供獨立的超音波感測器測試邏輯，
 * 用於在組裝或除錯時檢驗 Trig/Echo 接線、測距準確度、超時保護與中位數抗雜訊表現。
 */
namespace UltrasonicTest {

/**
 * @brief 超音波自檢啟動提示
 * @param sensor 待檢驗之超音波感測器實例引用
 */
inline void printStartupPrompt(const UltrasonicSensor& sensor) {
    Serial.println("\n--------------------------------------------------");
    Serial.println("[!] 目前處於【單一超音波獨立測試模式 (TEST_ULTRASONIC_ONLY = 1)】");
    Serial.printf ("[!] 測試目標感測器 ID: %d | 安裝角度: %4.1f 度\n", sensor.getId(), sensor.getAngleOffset());
    Serial.println("[!] 請在感測器前方移動障礙物，檢驗測距靈敏度與數值穩定性。");
    Serial.println("[!] 測試將在 2 秒後開始...");
    Serial.println("--------------------------------------------------");
    delay(1000); Serial.println("2...");
    delay(1000); Serial.println("1... START!\n");
    Serial.println("時間 (ms) | 單次測距 (Raw) | 中位數濾波 (Median) | 狀態與訊號條");
    Serial.println("----------------------------------------------------------------------");
}

/**
 * @brief 單一超音波感測器診斷循環
 * 依序執行單次即時測距與多次採樣中位數濾波測距，比對雜訊抑制效果並印出可視化數值
 * @param sensor 超音波感測器實例引用
 * @param sampleIntervalMs 兩次測量之間的間隔時間 (毫秒，預設 100ms)
 */
inline void runDiagnostic(UltrasonicSensor& sensor, uint32_t sampleIntervalMs = 100) {
    // 1. 執行單次原始測距 (Raw)
    float rawDist = sensor.measureDistance();

    // 2. 執行中位數濾波測距 (Median, 採樣 3 次, 間隔 10ms)
    float medianDist = sensor.measureMedian(3, 10);

    // 3. 狀態判斷與可視化距離條 (每 10cm 一格，最長 20 格)
    char bar[32] = {0};
    const char* statusStr = "VALID";

    if (rawDist < 0.0f && medianDist < 0.0f) {
        statusStr = "TIMEOUT / OUT_OF_RANGE";
        snprintf(bar, sizeof(bar), "[   OUT OF RANGE   ]");
    } else {
        float displayDist = (medianDist > 0.0f) ? medianDist : rawDist;
        int barLength = (int)(displayDist / 10.0f);
        if (barLength > 20) barLength = 20;

        bar[0] = '[';
        for (int i = 0; i < barLength; ++i) {
            bar[i + 1] = '=';
        }
        for (int i = barLength; i < 20; ++i) {
            bar[i + 1] = ' ';
        }
        bar[21] = ']';
        bar[22] = '\0';
    }

    // 4. 輸出除錯數據至 Serial Monitor
    if (rawDist > 0.0f && medianDist > 0.0f) {
        Serial.printf("%8lu ms | %6.1f cm      | %6.1f cm         | %-7s %s\n",
                      millis(), rawDist, medianDist, statusStr, bar);
    } else if (rawDist > 0.0f) {
        Serial.printf("%8lu ms | %6.1f cm      |    ---           | %-7s %s\n",
                      millis(), rawDist, statusStr, bar);
    } else if (medianDist > 0.0f) {
        Serial.printf("%8lu ms |    ---        | %6.1f cm         | %-7s %s\n",
                      millis(), medianDist, statusStr, bar);
    } else {
        Serial.printf("%8lu ms |    ---        |    ---           | %-7s %s\n",
                      millis(), statusStr, bar);
    }

    delay(sampleIntervalMs);
}

} // namespace UltrasonicTest

/**
 * @brief 相容性全域函式包裝 (方便直接呼叫)
 */
inline void printUltrasonicTestPrompt(const UltrasonicSensor& sensor) {
    UltrasonicTest::printStartupPrompt(sensor);
}

inline void runUltrasonicDiagnostic(UltrasonicSensor& sensor, uint32_t sampleIntervalMs = 100) {
    UltrasonicTest::runDiagnostic(sensor, sampleIntervalMs);
}
