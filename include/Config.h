#pragma once

#include <Arduino.h>

/**
 * ==============================================================================
 * Tracking Car - 專案全域配置檔 (Config.h)
 * ==============================================================================
 * 本檔案定義了 ESP32 腳位配置、硬體參數及預設常數。
 * 避免使用 GPIO 6~11 (內部 Flash) 以及開機 Strapping pins (GPIO 0, 2, 12, 15)。
 * GPIO 34, 35, 36, 39 為僅限輸入腳位 (Input Only)，可作為 Echo。
 */

namespace Config {

    // ==========================================
    // 1. 馬達驅動腳位設定 (4線式 H橋控制: A1A, A1B, B1A, B1B)
    // ==========================================
    // 適用於 L9110S / HG7881 等 4 接口馬達驅動模組
    // Motor A 預設作為左輪，Motor B 預設作為右輪 (若車體配置相反可於此對調腳位或在程式中設定翻轉)
    struct MotorPins {
        // 馬達 A (Motor A - 左馬達)
        static constexpr int8_t PIN_A1A = 18; // 控制腳 A-1A (PWM)
        static constexpr int8_t PIN_A1B = 19; // 控制腳 A-1B (PWM)

        // 馬達 B (Motor B - 右馬達)
        static constexpr int8_t PIN_B1A = 16; // 控制腳 B-1A (PWM)
        static constexpr int8_t PIN_B1B = 17; // 控制腳 B-1B (PWM)
    };

    // 馬達運作參數
    struct MotorParams {
        static constexpr uint8_t DEFAULT_SPEED = 180;   // 預設前進速度 (0 ~ 255)
        static constexpr uint8_t MIN_SPEED     = 60;    // 克服摩擦力的最小啟動速度
        static constexpr uint8_t MAX_SPEED     = 255;   // 最高速度
        static constexpr float   DEFAULT_TRIM_LEFT  = 1.0f; // 左輪微調修正係數
        static constexpr float   DEFAULT_TRIM_RIGHT = 1.0f; // 右輪微調修正係數
    };

    // ==========================================
    // 2. 多路超音波陣列設定 (MCP23017 Trig + ESP32 獨立 Echo)
    // ==========================================
    struct UltrasonicArrayPins {
        static constexpr uint8_t SENSOR_COUNT = 8;

        // MCP23017 I2C 設定
        static constexpr uint8_t MCP_I2C_ADDR   = 0x20;   // 預設 I2C 位址 (A0=A1=A2=GND)
        static constexpr int8_t  I2C_SDA_PIN    = 21;     // ESP32 SDA 腳位
        static constexpr int8_t  I2C_SCL_PIN    = 22;     // ESP32 SCL 腳位
        static constexpr uint32_t I2C_CLOCK     = 400000; // 400kHz Fast Mode

        // 8 顆超音波 Trig 對應之 MCP23017 腳位 (GPA0~GPA7 為 0~7)
        static constexpr uint8_t TRIG_MCP_PINS[SENSOR_COUNT] = {
            0, 1, 2, 3, 4, 5, 6, 7
        };

        // 8 顆超音波 Echo 直接連接 ESP32，各腳位皆設定為輸入
        static constexpr uint8_t ECHO_GPIO_PINS[SENSOR_COUNT] = {
            26, 27, 32, 33, 34, 35, 36, 39
        };

        static constexpr uint32_t MIN_PING_INTERVAL_MS = 60;

        // 8 顆超音波方位角，依除錯網站上的安裝方向設定 (車頭前方為 0°)
        static constexpr float SENSOR_ANGLES[SENSOR_COUNT] = {
               0.0f, -42.0f,  42.0f, -90.0f,
              90.0f, 180.0f, -138.0f, 138.0f
        };

        // 陣列測距範圍參數
        static constexpr float MIN_DISTANCE_CM = 2.0f;
        static constexpr float MAX_DISTANCE_CM = 250.0f;
    };

    // ==========================================
    // 3. 追蹤與安全防撞距離設定
    // ==========================================
    struct TrackingParams {
        static constexpr float STOP_DISTANCE_CM     = 15.0f; // 緊急煞車距離 (低於此距離停止或後退)
        static constexpr float TARGET_DISTANCE_CM   = 20.0f; // 理想跟隨距離
        static constexpr float MAX_TRACK_DISTANCE_CM = 150.0f; // 超出此距離視為丟失目標
        static constexpr float MIN_TARGET_MOVEMENT_CM = 5.0f; // 單次掃描的最小局部距離變化
        static constexpr uint8_t TARGET_MOTION_SCORE_THRESHOLD = 2; // 連續移動證據門檻
    };

    // 序列埠鮑率
    static constexpr uint32_t SERIAL_BAUD_RATE = 115200;
}
