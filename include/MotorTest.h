#pragma once

#include <Arduino.h>
#include "MotorController.h"

/**
 * ==============================================================================
 * Tracking Car - 馬達診斷與自檢測試模組 (MotorTest.h)
 * ==============================================================================
 * 本模組提供獨立的硬體馬達自檢邏輯與測試提示流程，
 * 用於在組裝或除錯時依序驗證左右輪轉向、差速、煞車與接線正確性。
 */
namespace MotorTest {

/**
 * @brief 馬達自檢啟動提示與安全倒數
 */
inline void printStartupPrompt() {
    Serial.println("\n[!] 目前處於【馬達單獨測試模式 (TEST_MOTOR_ONLY = 1)】");
    Serial.println("[!] 請務必【將車輪架空 (離地)】，避免小車暴衝摔落！");
    Serial.println("[!] 測試將在 3 秒後開始...");
    delay(1000); Serial.println("3...");
    delay(1000); Serial.println("2...");
    delay(1000); Serial.println("1... START!");
}

/**
 * @brief 馬達自檢循迴測試程序
 * 依序測試 Motor A 單獨轉動、Motor B 單獨轉動、雙輪前進、雙輪後退、原地旋轉
 * @param motor 馬達控制器實例引用
 */
inline void runDiagnostic(MotorController& motor) {
    Serial.println("\n--------------------------------------------------");
    Serial.println(">>> [Step 1] 測試 Motor A (單獨正轉 2 秒)");
    Serial.println("    預期: 左輪 (Motor A) 向前轉動，右輪靜止");
    motor.drive(180, 0);
    delay(2000);
    motor.stop();
    delay(1000);

    Serial.println(">>> [Step 2] 測試 Motor B (單獨正轉 2 秒)");
    Serial.println("    預期: 右輪 (Motor B) 向前轉動，左輪靜止");
    motor.drive(0, 180);
    delay(2000);
    motor.stop();
    delay(1000);

    Serial.println(">>> [Step 3] 測試雙輪前進 (Forward 2 秒)");
    Serial.println("    預期: 兩輪同時向前運轉");
    motor.forward(180);
    delay(2000);
    motor.stop();
    delay(1000);

    Serial.println(">>> [Step 4] 測試雙輪後退 (Backward 2 秒)");
    Serial.println("    預期: 兩輪同時向後運轉");
    motor.backward(180);
    delay(2000);
    motor.stop();
    delay(1000);

    Serial.println(">>> [Step 5] 測試原地左旋 (Spin Left 1.5 秒)");
    Serial.println("    預期: 左輪後退、右輪前進 (車身逆時針自轉)");
    motor.spinLeft(180);
    delay(1500);
    motor.stop();
    delay(1000);

    Serial.println(">>> [Step 6] 測試原地右旋 (Spin Right 1.5 秒)");
    Serial.println("    預期: 左輪前進、右輪後退 (車身順時針自轉)");
    motor.spinRight(180);
    delay(1500);
    motor.stop();

    Serial.println(">>> [Done] 本輪馬達自檢測試結束，冷卻暫停 3 秒...");
    Serial.println("--------------------------------------------------");
    delay(3000);
}

} // namespace MotorTest

/**
 * @brief 相容性全域函式包裝 (方便直接呼叫)
 */
inline void printMotorTestPrompt() {
    MotorTest::printStartupPrompt();
}

inline void runMotorDiagnostic(MotorController& motor) {
    MotorTest::runDiagnostic(motor);
}
