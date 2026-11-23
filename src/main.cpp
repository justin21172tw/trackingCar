#include <Arduino.h>
#include "Config.h"
#include "MotorController.h"
#include "UltrasonicSensor.h"
#include "UltrasonicArray.h"
#include "MotorTest.h"
#include "UltrasonicTest.h"
#include "UltrasonicArrayTest.h"
#include "UltrasonicHarnessTest.h"

/**
 * ==============================================================================
 * Tracking Car - 跟隨車主程式 (main.cpp)
 * ==============================================================================
 * 
 * 【測試模式切換開關】：
 * - TEST_MOTOR_ONLY         設為 1：專用「馬達獨立自檢測試模式」(不依賴超音波，循環驗證馬達動作與方向)。
 * - TEST_ULTRASONIC_ONLY    設為 1：專用「單一超音波獨立測試模式」(ESP32 直控腳位，不啟動馬達)。
 * - TEST_ULTRASONIC_ARRAY   設為 1：專用「多路超音波陣列測試模式」(輪詢 8 顆超音波)。
 * - TEST_ULTRASONIC_HARNESS 設為 1：專用「總成線路單顆超音波測試模式」(通過 MCP23017+CD74HC4067 驗證線束與插槽)。
 * - 上述皆為 0：正式「超音波跟隨模式」(結合超音波測距自動控制馬達)。
 */
#define TEST_MOTOR_ONLY          0
#define TEST_ULTRASONIC_ONLY     0
#define TEST_ULTRASONIC_ARRAY    0
#define TEST_ULTRASONIC_HARNESS  1

// 實例化模組一：馬達控制器 (4接口：A1A, A1B, B1A, B1B，腳位取自 Config.h)
MotorController motor(
    Config::MotorPins::PIN_A1A,
    Config::MotorPins::PIN_A1B,
    Config::MotorPins::PIN_B1A,
    Config::MotorPins::PIN_B1B
);

// 實例化模組二：單一超音波感測器 (測試用車頭感測器，ID=0，角度=0度)
UltrasonicSensor frontSensor(
    0, // ID 0
    Config::UltrasonicSinglePin::TRIG_PIN,
    Config::UltrasonicSinglePin::ECHO_PIN,
    Config::UltrasonicSinglePin::ANGLE,
    Config::UltrasonicParams::MAX_DISTANCE_CM,
    Config::UltrasonicParams::MIN_DISTANCE_CM
);

// 實例化模組三：多路超音波陣列控制器 (CD74HC4067 + MCP23017，支援指定單顆測距)
UltrasonicArray ultrasonicArray;

// 非同步定時器變數 (供超音波跟隨模式使用)
unsigned long lastMeasureTime = 0;
const unsigned long MEASURE_INTERVAL_MS = 80;

void setup() {
    // 1. 初始化序列埠
    Serial.begin(Config::SERIAL_BAUD_RATE);
    delay(1000); // 等待序列埠就緒
    Serial.println();
    Serial.println("==================================================");
    Serial.println("   ESP32 Tracking Car - Hardware Diagnostics     ");
    Serial.println("==================================================");

    // 2. 初始化馬達模組
    motor.init();
    motor.setTrim(Config::MotorParams::DEFAULT_TRIM_LEFT, 
                  Config::MotorParams::DEFAULT_TRIM_RIGHT);

    // 若接線後發現馬達正反向相反，可取消下方註解啟用軟體翻轉修正：
    // motor.setInvert(true, false); // 第一個參數為 Motor A, 第二個為 Motor B

    Serial.println("[Setup] Motor Controller initialized.");

#if TEST_MOTOR_ONLY
    // 顯示馬達測試啟動提示與安全倒數
    MotorTest::printStartupPrompt();
#elif TEST_ULTRASONIC_ONLY
    // 初始化超音波模組並進入超音波測試提示
    frontSensor.init();
    Serial.println("[Setup] Ultrasonic Sensor initialized.");
    UltrasonicTest::printStartupPrompt(frontSensor);
#elif TEST_ULTRASONIC_ARRAY
    // 初始化多路超音波陣列 (CD74HC4067 + MCP23017)
    if (!ultrasonicArray.init()) {
        Serial.println("[ERROR] MCP23017 或 I2C 匯流排初始化失敗！請檢查 SDA/SCL 接線與位址。");
    } else {
        Serial.println("[Setup] Ultrasonic Array initialized successfully.");
        UltrasonicArrayTest::printStartupPrompt(ultrasonicArray);
    }
#elif TEST_ULTRASONIC_HARNESS
    // 初始化總成線路擴充硬體 (CD74HC4067 + MCP23017)
    if (!ultrasonicArray.init()) {
        Serial.println("[ERROR] MCP23017 或 I2C 匯流排初始化失敗！請檢查 SDA/SCL 接線與位址。");
    } else {
        Serial.println("[Setup] 總成線路硬體初始化完成。");
        UltrasonicHarnessTest::printStartupPrompt(ultrasonicArray);
    }
#else
    // 3. 初始化超音波模組進入正式跟隨模式
    frontSensor.init();
    Serial.println("[Setup] Ultrasonic Sensor initialized.");
    Serial.println("[Setup] System ready! Entering main loop...\n");
#endif
}

void loop() {
#if TEST_MOTOR_ONLY
    // 執行馬達診斷循環 (邏輯封裝於 MotorTest 模組)
    MotorTest::runDiagnostic(motor);
#elif TEST_ULTRASONIC_ONLY
    // 執行單一超音波診斷循環 (邏輯封裝於 UltrasonicTest 模組)
    UltrasonicTest::runDiagnostic(frontSensor);
#elif TEST_ULTRASONIC_ARRAY
    // 自訂順序指定 8 顆超音波測距（索引範圍 0~7，優先由車頭中央擴展至兩側）
    static const uint8_t dynamicOrder[] = { 4, 3, 5, 2, 6, 1, 7, 0 };
    static const uint8_t orderLen = sizeof(dynamicOrder) / sizeof(dynamicOrder[0]);

    UltrasonicArrayTest::runCustomSequenceDiagnostic(ultrasonicArray, dynamicOrder, orderLen, 200);
#elif TEST_ULTRASONIC_HARNESS
    // 執行總成線路單顆超音波診斷 (支援鎖定即時測距、Serial 切換插槽 0~7、自動尋標巡檢與檢驗清單)
    UltrasonicHarnessTest::runDiagnostic(ultrasonicArray);
#else
    // 超音波跟隨主邏輯
    unsigned long currentMillis = millis();

    if (currentMillis - lastMeasureTime >= MEASURE_INTERVAL_MS) {
        lastMeasureTime = currentMillis;

        float distance = frontSensor.measureDistance();
        const char* statusStr = "UNKNOWN";

        if (distance < 0.0f) {
            statusStr = "NO_TARGET_STOP";
            motor.stop();
        } 
        else if (distance < Config::TrackingParams::STOP_DISTANCE_CM) {
            statusStr = "TOO_CLOSE_BRAKE";
            motor.brake();
        } 
        else if (distance >= Config::TrackingParams::STOP_DISTANCE_CM && 
                 distance <= (Config::TrackingParams::TARGET_DISTANCE_CM + 5.0f)) {
            statusStr = "TARGET_FOLLOW_SLOW";
            motor.forward(120);
        } 
        else if (distance > (Config::TrackingParams::TARGET_DISTANCE_CM + 5.0f) && 
                 distance <= Config::TrackingParams::MAX_TRACK_DISTANCE_CM) {
            statusStr = "TARGET_FOLLOW_NORMAL";
            motor.forward(Config::MotorParams::DEFAULT_SPEED);
        } 
        else {
            statusStr = "TARGET_LOST_STOP";
            motor.stop();
        }

        // 輸出除錯數據至 Serial Monitor
        Serial.printf("[Sensor ID:%d | Ang:%4.1f deg] Dist: %6.1f cm | Status: %-20s | Motor: A=%4d, B=%4d\n",
                      frontSensor.getId(),
                      frontSensor.getAngleOffset(),
                      distance,
                      statusStr,
                      motor.getSpeedA(),
                      motor.getSpeedB());
    }
#endif
}
