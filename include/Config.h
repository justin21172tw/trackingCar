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
        static constexpr int8_t PIN_B1A = 22; // 控制腳 B-1A (PWM)
        static constexpr int8_t PIN_B1B = 23; // 控制腳 B-1B (PWM)
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
    // 2. 超音波感測器腳位設定 (Ultrasonic Sensor)
    // ==========================================
    // 單一測試用超音波腳位 (通常置於車頭正中央，角度 0.0 度)
    struct UltrasonicSinglePin {
        static constexpr uint8_t TRIG_PIN = 5;
        static constexpr uint8_t ECHO_PIN = 17;
        static constexpr float   ANGLE    = 0.0f; // 車頭正前方為 0 度
    };

    // 超音波感測參數
    struct UltrasonicParams {
        static constexpr float MIN_DISTANCE_CM = 2.0f;    // 最小可量測距離 (cm)
        static constexpr float MAX_DISTANCE_CM = 300.0f;  // 最大可量測距離 (cm)
        static constexpr float SOUND_SPEED_CM_US = 0.0343f; // 音速: 343 m/s = 0.0343 cm/us
        static constexpr uint32_t TIMEOUT_US   = 20000;   // pulseIn 超時 (us)，約對應 340cm
    };

    // ==========================================
    // 3. 追蹤與安全防撞距離設定 (後續演算法使用)
    // ==========================================
    struct TrackingParams {
        static constexpr float STOP_DISTANCE_CM     = 15.0f; // 緊急煞車距離 (低於此距離停止或後退)
        static constexpr float TARGET_DISTANCE_CM   = 30.0f; // 理想跟隨距離
        static constexpr float MAX_TRACK_DISTANCE_CM = 150.0f; // 超出此距離視為丟失目標
    };

    // 序列埠鮑率
    static constexpr uint32_t SERIAL_BAUD_RATE = 115200;
}
