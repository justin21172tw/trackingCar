#pragma once

#include <Arduino.h>
#include "UltrasonicArray.h"
#include "Config.h"

/**
 * ==============================================================================
 * Tracking Car - 總成線路單顆超音波診斷與線束檢測模組 (UltrasonicHarnessTest.h)
 * ==============================================================================
 * 
 * 【測試背景與目標】：
 * 在整車組裝或線束焊接完成後，使用者通常需要檢驗小車的「總成線路」
 * (包含 ESP32 <-> MCP23017 <-> CD74HC4067 <-> 各超音波插槽/端子)。
 * 
 * 此時手邊往往只有「隨便一顆超音波偵測器」，本模組專門針對此情境設計：
 * 1. 【鎖定單一插槽模式 (預設)】：
 *    - 鎖定單一特定插槽 (預設 #0)，以 12~15Hz 高更新率連續即時量測。
 *    - 不會因其他 7 個未接感測器的插槽超時而造成主迴圈卡頓 (每次僅耗時 2~15ms)。
 *    - 輸出 Echo 微秒脈衝時間、即時公分測距、中位數濾波值與視覺化距離條。
 *    - 支援 Serial 輸入 0~7 隨時無縫切換當前插槽，方便將同一顆超音波插到不同孔位測試。
 * 
 * 2. 【全插槽自動尋標巡檢模式】：
 *    - 輸入 's' 或 'a' 啟用。自動快速輪詢 8 個插槽。
 *    - 當偵測到使用者將超音波插入任一插槽且測得有效回波時，立即標記並提示「該插槽線路合格 [PASS]」。
 *    - 內建「總成線路檢驗清單 (Checklist)」，協助依序插拔驗證全部 8 組線路。
 * 
 * 3. 【硬體故障診斷與引導】：
 *    - 啟動時即時檢測 I2C 與 MCP23017 通訊狀態。
 *    - 檢驗 GPIO 34 (Echo 總線) 靜態電位，防範接地不良或短路浮接。
 *    - 連續超時自動給出對應插槽的硬體排錯指南 (Trig GPAx / Echo Cx / 供電)。
 */

namespace UltrasonicHarnessTest {

// ==============================================================================
// 內部狀態變數
// ==============================================================================
static uint8_t  s_targetSocket       = 0;     // 當前鎖定之測試插槽編號 (0 ~ 7)
static bool     s_autoScanMode       = false; // 是否處於自動尋標模式
static bool     s_verifiedSockets[8] = {false}; // 8 組插槽是否已通過檢驗
static float    s_lastDistances[8]   = {0.0f};  // 各插槽最近一次通過時的測距
static uint32_t s_timeoutCounter     = 0;     // 連續超時次數計數器
static uint32_t s_lastLoopTimestamp  = 0;     // 測量計時器

/**
 * @brief 印出指令說明選單
 */
inline void printHelp() {
    Serial.println("\n----------------------------------------------------------------------");
    Serial.println("【序列埠即時互動指令選單】(於 Serial Monitor 輸入後送出):");
    Serial.println("  [0] ~ [7] : 立即切換並鎖定檢測【插槽 #0 ~ #7】(高更新率即時量測)");
    Serial.println("  [s] 或 [a]: 切換【全插槽自動尋標巡檢模式】(自動尋找有接線的插槽)");
    Serial.println("  [c]       : 顯示 8 組總成插槽之檢驗通關進度清單 (Checklist)");
    Serial.println("  [r]       : 重設檢驗進度清單");
    Serial.println("  [h] 或 [?]: 重新顯示此說明選單");
    Serial.println("----------------------------------------------------------------------\n");
}

/**
 * @brief 顯示目前 8 組插槽之線路檢驗清單
 */
inline void printChecklist(const UltrasonicArray& array) {
    uint8_t total = array.getSensorCount();
    uint8_t passedCount = 0;

    Serial.println("\n======================================================================");
    Serial.println("                小車總成線路檢驗通關清單 (Harness Checklist)          ");
    Serial.println("======================================================================");
    Serial.println(" 插槽編號 | 安裝方位角 | 硬體接線對應               | 檢驗狀態 | 最近測距");
    Serial.println("----------------------------------------------------------------------");

    for (uint8_t i = 0; i < total; ++i) {
        float angle = array.getSensorAngle(i);
        uint8_t trigPin = array.getTrigPin(i);
        uint8_t echoCh  = array.getEchoChannel(i);
        bool passed = s_verifiedSockets[i];
        if (passed) passedCount++;

        char statusStr[32];
        if (passed) {
            snprintf(statusStr, sizeof(statusStr), "[ PASS  合格 ]");
        } else {
            snprintf(statusStr, sizeof(statusStr), "[ PENDING 待測]");
        }

        if (passed && s_lastDistances[i] > 0.0f) {
            Serial.printf(" 插槽 #%02d   | %5.1f°     | Trig: GPA%d  | Echo: MUX C%d  | %-14s | %5.1f cm\n",
                          i, angle, trigPin, echoCh, statusStr, s_lastDistances[i]);
        } else {
            Serial.printf(" 插槽 #%02d   | %5.1f°     | Trig: GPA%d  | Echo: MUX C%d  | %-14s |    ---   \n",
                          i, angle, trigPin, echoCh, statusStr);
        }
    }

    Serial.println("----------------------------------------------------------------------");
    Serial.printf(" >> 總體進度: [ %d / %d ] 個插槽線路已驗證通過 (%.0f%%)\n",
                  passedCount, total, (float)passedCount / total * 100.0f);

    if (passedCount == total) {
        Serial.println(" >> 🎉【全數通關】恭喜！全部 8 組總成插槽線路與供電皆完全正常！");
    } else {
        Serial.printf(" >> 提示：請將超音波拔出並換插至其他待測插槽，繼續驗證未通過線路。\n");
    }
    Serial.println("======================================================================\n");
}

/**
 * @brief 重設檢驗清單紀錄
 */
inline void resetChecklist() {
    for (uint8_t i = 0; i < 8; ++i) {
        s_verifiedSockets[i] = false;
        s_lastDistances[i]   = 0.0f;
    }
    s_timeoutCounter = 0;
    Serial.println("\n[系統通知] 總成插槽線路檢驗清單已全數重設為【待測】狀態。\n");
}

/**
 * @brief 印出啟動自檢畫面與硬體線路診斷資訊
 */
inline void printStartupPrompt(UltrasonicArray& array) {
    Serial.println("\n======================================================================");
    Serial.println("          ESP32 Tracking Car - 總成線路單顆超音波檢測模組             ");
    Serial.println("======================================================================");
    Serial.println("[!] 測試目的：使用「任意單顆超音波」，完整驗證小車多路總成線路與插槽");
    Serial.println("[!] 硬體架構：ESP32 <-> MCP23017 (Trig GPA0~7) + CD74HC4067 (Echo C0~7)");
    Serial.println("----------------------------------------------------------------------");

    // 1. 檢驗 MCP23017 I2C 通訊
    bool i2cOk = array.getMCP().isConnected();
    if (i2cOk) {
        Serial.printf("[硬體自檢] [ OK ] MCP23017 I2C (0x%02X) 通訊連線正常！\n",
                      Config::UltrasonicArrayPins::MCP_I2C_ADDR);
    } else {
        Serial.println("[硬體自檢] [FAIL] MCP23017 I2C 通訊失敗！");
        Serial.println("           >> 排錯指南：");
        Serial.println("           1. 請確認 ESP32 SDA (GPIO 21) 與 SCL (GPIO 4) 接線是否正確");
        Serial.println("           2. 請確認 MCP23017 VDD (Pin 9) / VSS (Pin 10) 供電正常");
        Serial.println("           3. 請確認 MCP23017 RESET (Pin 18) 已接至 HIGH (VDD)");
        Serial.println("           4. 請確認 I2C 具備 4.7kΩ 上拉電阻");
    }

    // 2. 檢驗 Echo 總線靜態電位
    uint8_t echoPin = array.getEchoPin();
    int echoBaseline = digitalRead(echoPin);
    if (echoBaseline == LOW) {
        Serial.printf("[硬體自檢] [ OK ] ESP32 Echo 總線 (GPIO %d) 靜態電位正常 (LOW)\n", echoPin);
    } else {
        Serial.printf("[硬體自檢] [WARN] ESP32 Echo 總線 (GPIO %d) 靜態電位異常為 HIGH！\n", echoPin);
        Serial.println("           >> 請檢查 CD74HC4067 SIG 輸出線路、是否短路或浮接未接地");
    }

    // 3. 輸出指令與預設狀態提示
    printHelp();

    Serial.printf("[預設狀態] 已鎖定【插槽 #%d】進行高更新率量測。\n", s_targetSocket);
    Serial.printf("           (Trig 接 MCP23017 GPA%d | Echo 接 CD74HC4067 C%d | 方位角 %4.1f°)\n",
                  array.getTrigPin(s_targetSocket),
                  array.getEchoChannel(s_targetSocket),
                  array.getSensorAngle(s_targetSocket));
    Serial.println("           若您的超音波插在其他孔位，請直接在 Serial 輸入 0~7 切換！\n");
    Serial.println("時間 (ms) | 插槽 | 硬體腳位對應        | 脈衝寬度 | 單次 (Raw) | 濾波 (Median) | 訊號強度條與狀態");
    Serial.println("------------------------------------------------------------------------------------------------");
}

/**
 * @brief 處理來自 Serial Monitor 的互動輸入指令
 */
inline void handleSerialCommands(UltrasonicArray& array) {
    while (Serial.available() > 0) {
        char ch = (char)Serial.read();

        // 忽略換行與空白
        if (ch == '\r' || ch == '\n' || ch == ' ') continue;

        // 數字鍵 0 ~ 7: 切換指定插槽
        if (ch >= '0' && ch <= '7') {
            uint8_t newSock = ch - '0';
            if (newSock < array.getSensorCount()) {
                s_targetSocket = newSock;
                s_autoScanMode = false;
                s_timeoutCounter = 0;
                Serial.printf("\n>>> [手動切換] 鎖定檢測【插槽 #%d】(Trig: MCP GPA%d | Echo: MUX C%d | 角度: %4.1f°)\n",
                              s_targetSocket,
                              array.getTrigPin(s_targetSocket),
                              array.getEchoChannel(s_targetSocket),
                              array.getSensorAngle(s_targetSocket));
            }
        }
        // 切換自動尋標模式
        else if (ch == 's' || ch == 'S' || ch == 'a' || ch == 'A') {
            s_autoScanMode = !s_autoScanMode;
            s_timeoutCounter = 0;
            if (s_autoScanMode) {
                Serial.println("\n>>> [模式切換] 啟用【全插槽自動尋標巡檢模式】(自動輪詢 0~7 尋找在線插槽)...");
            } else {
                Serial.printf("\n>>> [模式切換] 恢復【鎖定插槽 #%d 高速檢測模式】\n", s_targetSocket);
            }
        }
        // 查看進度清單
        else if (ch == 'c' || ch == 'C') {
            printChecklist(array);
        }
        // 重設清單
        else if (ch == 'r' || ch == 'R') {
            resetChecklist();
        }
        // 說明選單
        else if (ch == 'h' || ch == 'H' || ch == '?') {
            printHelp();
        }
    }
}

/**
 * @brief 繪製距離可視化長條字串 (每 10cm 一格，上限 20 格)
 */
inline void buildVisualBar(float distance, char* outBar, size_t barSize) {
    if (barSize < 24) return;
    outBar[0] = '[';
    if (distance > 0.0f) {
        int len = (int)(distance / 10.0f);
        if (len > 20) len = 20;
        for (int i = 0; i < len; ++i) outBar[i + 1] = '=';
        for (int i = len; i < 20; ++i) outBar[i + 1] = ' ';
    } else {
        for (int i = 0; i < 20; ++i) outBar[i + 1] = ' ';
    }
    outBar[21] = ']';
    outBar[22] = '\0';
}

/**
 * @brief 單一超音波總成線路主診斷循環
 * 需置於 main.cpp 的 loop() 中呼叫
 * @param array 超音波陣列實例
 * @param intervalMs 單次更新頻率間隔 (毫秒，預設 70ms，提供平滑即時回饋)
 */
inline void runDiagnostic(UltrasonicArray& array, uint32_t intervalMs = 70) {
    // 1. 處理 Serial Monitor 即時輸入指令
    handleSerialCommands(array);

    // 2. 定時器節流控制
    uint32_t now = millis();
    if (now - s_lastLoopTimestamp < intervalMs) {
        return;
    }
    s_lastLoopTimestamp = now;

    // 3. 確保 I2C 處於連線狀態
    if (!array.getMCP().isConnected()) {
        static uint32_t s_lastI2cWarning = 0;
        if (now - s_lastI2cWarning > 2000) {
            Serial.println("[ERROR] MCP23017 晶片無回應！請檢查 I2C SDA/SCL 總成接線或重插小車電源。");
            s_lastI2cWarning = now;
        }
        return;
    }

    // =========================================================================
    // 分支 A: 全插槽自動尋標巡檢模式 (Auto-Scan)
    // =========================================================================
    if (s_autoScanMode) {
        uint8_t totalSensors = array.getSensorCount();
        bool foundAny = false;

        for (uint8_t i = 0; i < totalSensors; ++i) {
            unsigned long durUs = array.measureEchoDurationUs(i);
            if (durUs > 0) {
                float dist = durUs * (0.0343f / 2.0f);
                if (dist >= array.getMinDistanceCm() && dist <= array.getMaxDistanceCm()) {
                    foundAny = true;
                    s_lastDistances[i] = dist;

                    bool firstPass = !s_verifiedSockets[i];
                    s_verifiedSockets[i] = true;

                    char bar[24];
                    buildVisualBar(dist, bar, sizeof(bar));

                    if (firstPass) {
                        Serial.printf("\n>>> 🎉 [新插槽檢驗合格！] 偵測到感測器位於【插槽 #%d】| 測距: %5.1f cm (脈衝: %5lu us) %s\n",
                                      i, dist, durUs, bar);
                        Serial.printf("    對應總成線路: Trig -> MCP GPA%d | Echo -> CD74HC4067 C%d | 角度 %4.1f° [PASS]\n\n",
                                      array.getTrigPin(i), array.getEchoChannel(i), array.getSensorAngle(i));
                    } else {
                        Serial.printf("%8lu ms | #%02d  | Trig:GPA%d Echo:C%d | %5lu us | %5.1f cm   |    ---        | %s [PASS]\n",
                                      now, i, array.getTrigPin(i), array.getEchoChannel(i), durUs, dist, bar);
                    }
                }
            }
            delay(10); // 短暫延遲避免相鄰頻道餘波
        }

        if (!foundAny) {
            s_timeoutCounter++;
            if (s_timeoutCounter % 30 == 0) {
                Serial.printf("%8lu ms | [掃描中] 8 組插槽皆無回波... 請確認感測器已插牢或前方 30cm 內放置障礙物 (輸入 '0'~'7' 可鎖定特定孔)\n", now);
            }
        }
        return;
    }

    // =========================================================================
    // 分支 B: 鎖定指定插槽連續高速測試模式 (Locked Socket Mode)
    // =========================================================================
    uint8_t sock = s_targetSocket;
    uint8_t trigPin = array.getTrigPin(sock);
    uint8_t echoCh  = array.getEchoChannel(sock);

    // 1. 測量原始 Echo 脈衝寬度
    unsigned long durationUs = array.measureEchoDurationUs(sock);

    // 2. 測量多次採樣中位數濾波 (消除環境雜訊)
    float medianDist = (durationUs > 0) ? array.measureMedian(sock, 3, 10) : -1.0f;
    float rawDist    = (durationUs > 0) ? (durationUs * (0.0343f / 2.0f)) : -1.0f;

    // 檢查範圍有效性
    if (rawDist < array.getMinDistanceCm() || rawDist > array.getMaxDistanceCm()) {
        rawDist = -1.0f;
    }

    // 3. 視覺化長條
    char bar[24];
    float displayDist = (medianDist > 0.0f) ? medianDist : rawDist;
    buildVisualBar(displayDist, bar, sizeof(bar));

    // 4. 結果輸出與線路狀態判定
    if (displayDist > 0.0f) {
        s_timeoutCounter = 0;
        s_verifiedSockets[sock] = true;
        s_lastDistances[sock] = displayDist;

        Serial.printf("%8lu ms | #%02d  | Trig:GPA%d Echo:C%d | %5lu us | %5.1f cm   | %5.1f cm      | %s [PASS]\n",
                      now, sock, trigPin, echoCh, durationUs, rawDist, medianDist, bar);
    } else {
        s_timeoutCounter++;
        Serial.printf("%8lu ms | #%02d  | Trig:GPA%d Echo:C%d |    0 us |    ---     |    ---        | [ TIMEOUT / NO ECHO ]\n",
                      now, sock, trigPin, echoCh);

        // 每累積 20 次超時 (約 1.5 秒)，輸出該插槽的硬體排錯指南
        if (s_timeoutCounter == 20) {
            Serial.println("\n----------------------------------------------------------------------");
            Serial.printf(">> [線路排錯指引 - 插槽 #%d]:\n", sock);
            Serial.printf("   若您已將超音波插入【插槽 #%d】，卻持續顯示 TIMEOUT，請檢查以下總成線路：\n", sock);
            Serial.printf("   1. [Trig 觸發線]: MCP23017 GPA%d (Pin %d) 到此插槽的走線是否導通？\n", trigPin, trigPin + 1);
            Serial.printf("   2. [Echo 回波線]: CD74HC4067 C%d (通道 %d) 到此插槽的走線是否導通？\n", echoCh, echoCh);
            Serial.println("   3. [電源與地線]: 插槽端子的 5V (VCC) 與 GND 是否正常供電？");
            Serial.println("   4. [障礙物]: 請在感測器前方 10~50cm 處放置手掌或書本，避免角度過斜造成聲波散射。");
            Serial.println("   (若超音波插在其他孔位，請直接鍵盤輸入 '0'~'7' 切換插槽，或按 's' 開啟自動尋標)");
            Serial.println("----------------------------------------------------------------------\n");
        }
    }
}

} // namespace UltrasonicHarnessTest
