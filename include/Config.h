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
    // 3. 多路超音波陣列設定 (CD74HC4067 複用器 + MCP23017 I2C 擴充)
    // ==========================================
    struct UltrasonicArrayPins {
        static constexpr uint8_t SENSOR_COUNT = 10;

        // MCP23017 I2C 設定
        static constexpr uint8_t MCP_I2C_ADDR   = 0x20;   // 預設 I2C 位址 (A0=A1=A2=GND)
        static constexpr int8_t  I2C_SDA_PIN    = 21;     // ESP32 SDA 腳位
        static constexpr int8_t  I2C_SCL_PIN    = 4;      // ESP32 SCL 腳位 (避開與 Motor B1A: GPIO 22 衝突)
        static constexpr uint32_t I2C_CLOCK     = 400000; // 400kHz Fast Mode

        // CD74HC4067 多路複用器 (MUX) 控制模式
        // true: 由 MCP23017 腳位控制 S0~S3 (節省 ESP32 腳位，僅需 I2C+Echo 共 3 腳)
        // false: 由 ESP32 GPIO 直接控制 S0~S3 (極速切換)
        static constexpr bool MUX_CONTROL_VIA_MCP = true;

        // 當 MUX_CONTROL_VIA_MCP == false (由 ESP32 GPIO 直控 S0~S3)
        static constexpr int8_t ESP32_MUX_S0 = 13;
        static constexpr int8_t ESP32_MUX_S1 = 14;
        static constexpr int8_t ESP32_MUX_S2 = 27;
        static constexpr int8_t ESP32_MUX_S3 = 26;
        static constexpr int8_t ESP32_MUX_EN = -1; // -1 表示實體接 GND 常時使能

        // 當 MUX_CONTROL_VIA_MCP == true (由 MCP23017 控制 S0~S3: GPB4~GPB7 為 Pin 12~15)
        static constexpr uint8_t MCP_MUX_S0  = 12; // GPB4
        static constexpr uint8_t MCP_MUX_S1  = 13; // GPB5
        static constexpr uint8_t MCP_MUX_S2  = 14; // GPB6
        static constexpr uint8_t MCP_MUX_S3  = 15; // GPB7
        static constexpr int8_t  MCP_MUX_EN  = -1; // -1 表示實體接 GND

        // CD74HC4067 SIG (COM) 總回波線接至 ESP32 (建議使用僅輸入腳位 GPIO 34, 35, 36, 39)
        static constexpr uint8_t ESP32_ECHO_PIN = 34;

        // 10 顆超音波 Trig 對應之 MCP23017 腳位 (GPA0~GPA7 為 0~7, GPB0~GPB1 為 8~9)
        static constexpr uint8_t TRIG_MCP_PINS[SENSOR_COUNT] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9
        };

        // 10 顆超音波 Echo 對應之 CD74HC4067 頻道 (0 ~ 9)
        static constexpr uint8_t ECHO_MUX_CHANNELS[SENSOR_COUNT] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9
        };

        // 10 顆超音波方位角 (度，車頭正中央為 0°，負值偏左，正值偏右，扇形排列)
        static constexpr float SENSOR_ANGLES[SENSOR_COUNT] = {
            -75.0f, -55.0f, -38.0f, -22.0f, -7.0f,
              7.0f,  22.0f,  38.0f,  55.0f, 75.0f
        };

        // 陣列測距範圍參數
        static constexpr float MIN_DISTANCE_CM = 2.0f;
        static constexpr float MAX_DISTANCE_CM = 250.0f;
    };

    // ==========================================
    // 4. 追蹤與安全防撞距離設定 (後續演算法使用)
    // ==========================================
    struct TrackingParams {
        static constexpr float STOP_DISTANCE_CM     = 15.0f; // 緊急煞車距離 (低於此距離停止或後退)
        static constexpr float TARGET_DISTANCE_CM   = 30.0f; // 理想跟隨距離
        static constexpr float MAX_TRACK_DISTANCE_CM = 150.0f; // 超出此距離視為丟失目標
    };

    // 序列埠鮑率
    static constexpr uint32_t SERIAL_BAUD_RATE = 115200;
}
