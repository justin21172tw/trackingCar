#pragma once

#include <Arduino.h>

/**
 * @file MotorController.h
 * @brief 模組一：雙通道 H 橋馬達控制器 (Motor A & Motor B)
 * 
 * 適用於具備 A1A, A1B, B1A, B1B 四接口之直流馬達驅動模組 (如 L9110S / HG7881 等)。
 * - Motor A 控制接口：A1A, A1B
 * - Motor B 控制接口：B1A, B1B
 */

class MotorController {
public:
    /**
     * @brief 建構子
     * @param pinA1A 馬達 A 控制腳位 A1A
     * @param pinA1B 馬達 A 控制腳位 A1B
     * @param pinB1A 馬達 B 控制腳位 B1A
     * @param pinB1B 馬達 B 控制腳位 B1B
     */
    MotorController(int8_t pinA1A, int8_t pinA1B, int8_t pinB1A, int8_t pinB1B);

    /**
     * @brief 初始化馬達 GPIO 腳位並預設停止
     */
    void init();

    /**
     * @brief 設定左右輪修偏係數 (Trim)，解決硬體馬達轉速不一問題
     * @param trimA 馬達 A 修偏係數 (預設 1.0)
     * @param trimB 馬達 B 修偏係數 (預設 1.0)
     */
    void setTrim(float trimA, float trimB);

    /**
     * @brief 設定馬達方向是否反轉 (若接線與預期旋轉方向相反時使用，免重新插拔線)
     * @param invertA 是否反轉馬達 A 方向
     * @param invertB 是否反轉馬達 B 方向
     */
    void setInvert(bool invertA, bool invertB);

    /**
     * @brief 直接設定 Motor A 與 Motor B 速度
     * @param speedA 馬達 A 請求速度 (-255 ~ 255，正值前進，負值後退，0 停止；實際輸出受 MAX_SPEED 限制)
     * @param speedB 馬達 B 請求速度 (-255 ~ 255，正值前進，負值後退，0 停止；實際輸出受 MAX_SPEED 限制)
     */
    void drive(int speedA, int speedB);

    /**
     * @brief 雙輪前進 (Motor A 與 Motor B 同步正轉)
     * @param speed 速度 (0 ~ 255)
     */
    void forward(uint8_t speed);

    /**
     * @brief 雙輪後退 (Motor A 與 Motor B 同步反轉)
     * @param speed 速度 (0 ~ 255)
     */
    void backward(uint8_t speed);

    /**
     * @brief 差速左轉 (雙輪前進，Motor A [左輪] 降速)
     * @param speed 基礎速度 (0 ~ 255)
     * @param turnRatio 內側輪 (Motor A) 速度比例 (0.0: 內側輪停止, 1.0: 等速直行)
     */
    void turnLeft(uint8_t speed, float turnRatio = 0.4f);

    /**
     * @brief 差速右轉 (雙輪前進，Motor B [右輪] 降速)
     * @param speed 基礎速度 (0 ~ 255)
     * @param turnRatio 內側輪 (Motor B) 速度比例 (0.0: 內側輪停止, 1.0: 等速直行)
     */
    void turnRight(uint8_t speed, float turnRatio = 0.4f);

    /**
     * @brief 原地原地左旋 (Motor A 後退，Motor B 前進)
     * @param speed 旋轉速度 (0 ~ 255)
     */
    void spinLeft(uint8_t speed);

    /**
     * @brief 原地原地右旋 (Motor A 前進，Motor B 後退)
     * @param speed 旋轉速度 (0 ~ 255)
     */
    void spinRight(uint8_t speed);

    /**
     * @brief 自然滑行停止 (四個接口皆輸出 LOW，解除動力)
     */
    void stop();

    /**
     * @brief 主動緊急煞車 (四個接口皆輸出 HIGH，短路制動)
     */
    void brake();

    /**
     * @brief 取得當前 Motor A PWM 輸出命令 (受 Config::MotorParams::MAX_SPEED 限制)
     */
    int getSpeedA() const { return _currentSpeedA; }

    /**
     * @brief 取得當前 Motor B PWM 輸出命令 (受 Config::MotorParams::MAX_SPEED 限制)
     */
    int getSpeedB() const { return _currentSpeedB; }

    // 相容左/右輪語意取得速度
    int getCurrentLeftSpeed() const { return _currentSpeedA; }
    int getCurrentRightSpeed() const { return _currentSpeedB; }

private:
    int8_t _pinA1A;
    int8_t _pinA1B;
    int8_t _pinB1A;
    int8_t _pinB1B;

    float _trimA;
    float _trimB;

    bool _invertA;
    bool _invertB;

    int _currentSpeedA;
    int _currentSpeedB;

    /**
     * @brief 獨立驅動 Motor A
     * @param speed 速度 (-255 ~ 255)
     */
    void setMotorA(int speed);

    /**
     * @brief 獨立驅動 Motor B
     * @param speed 速度 (-255 ~ 255)
     */
    void setMotorB(int speed);
};
