#include "MotorController.h"

MotorController::MotorController(int8_t pinA1A, int8_t pinA1B, int8_t pinB1A, int8_t pinB1B)
    : _pinA1A(pinA1A), _pinA1B(pinA1B),
      _pinB1A(pinB1A), _pinB1B(pinB1B),
      _trimA(1.0f), _trimB(1.0f),
      _invertA(false), _invertB(false),
      _currentSpeedA(0), _currentSpeedB(0) {
}

void MotorController::init() {
    // 初始化 Motor A 接口
    pinMode(_pinA1A, OUTPUT);
    pinMode(_pinA1B, OUTPUT);
    digitalWrite(_pinA1A, LOW);
    digitalWrite(_pinA1B, LOW);

    // 初始化 Motor B 接口
    pinMode(_pinB1A, OUTPUT);
    pinMode(_pinB1B, OUTPUT);
    digitalWrite(_pinB1A, LOW);
    digitalWrite(_pinB1B, LOW);

    stop();
}

void MotorController::setTrim(float trimA, float trimB) {
    _trimA = constrain(trimA, 0.0f, 2.0f);
    _trimB = constrain(trimB, 0.0f, 2.0f);
}

void MotorController::setInvert(bool invertA, bool invertB) {
    _invertA = invertA;
    _invertB = invertB;
}

void MotorController::setMotorA(int speed) {
    if (_invertA) {
        speed = -speed;
    }

    int magnitude = constrain(abs(speed), 0, 255);

    if (speed > 0) {
        // Motor A 正轉: A1A 輸出 PWM, A1B 保持 LOW
        analogWrite(_pinA1A, magnitude);
        analogWrite(_pinA1B, 0);
    } else if (speed < 0) {
        // Motor A 反轉: A1A 保持 LOW, A1B 輸出 PWM
        analogWrite(_pinA1A, 0);
        analogWrite(_pinA1B, magnitude);
    } else {
        // Motor A 停止: 兩端皆 LOW
        analogWrite(_pinA1A, 0);
        analogWrite(_pinA1B, 0);
    }
}

void MotorController::setMotorB(int speed) {
    if (_invertB) {
        speed = -speed;
    }

    int magnitude = constrain(abs(speed), 0, 255);

    if (speed > 0) {
        // Motor B 正轉: B1A 輸出 PWM, B1B 保持 LOW
        analogWrite(_pinB1A, magnitude);
        analogWrite(_pinB1B, 0);
    } else if (speed < 0) {
        // Motor B 反轉: B1A 保持 LOW, B1B 輸出 PWM
        analogWrite(_pinB1A, 0);
        analogWrite(_pinB1B, magnitude);
    } else {
        // Motor B 停止: 兩端皆 LOW
        analogWrite(_pinB1A, 0);
        analogWrite(_pinB1B, 0);
    }
}

void MotorController::drive(int speedA, int speedB) {
    speedA = constrain(speedA, -255, 255);
    speedB = constrain(speedB, -255, 255);

    // 套用修偏比例
    int adjustedA = constrain((int)(speedA * _trimA), -255, 255);
    int adjustedB = constrain((int)(speedB * _trimB), -255, 255);

    _currentSpeedA = adjustedA;
    _currentSpeedB = adjustedB;

    setMotorA(adjustedA);
    setMotorB(adjustedB);
}

void MotorController::forward(uint8_t speed) {
    drive(speed, speed);
}

void MotorController::backward(uint8_t speed) {
    drive(-speed, -speed);
}

void MotorController::turnLeft(uint8_t speed, float turnRatio) {
    turnRatio = constrain(turnRatio, -1.0f, 1.0f);
    int innerSpeed = (int)(speed * turnRatio);
    drive(innerSpeed, speed); // Motor A (左) 降速，Motor B (右) 等速
}

void MotorController::turnRight(uint8_t speed, float turnRatio) {
    turnRatio = constrain(turnRatio, -1.0f, 1.0f);
    int innerSpeed = (int)(speed * turnRatio);
    drive(speed, innerSpeed); // Motor A (左) 等速，Motor B (右) 降速
}

void MotorController::spinLeft(uint8_t speed) {
    drive(-speed, speed); // Motor A 後退，Motor B 前進
}

void MotorController::spinRight(uint8_t speed) {
    drive(speed, -speed); // Motor A 前進，Motor B 後退
}

void MotorController::stop() {
    _currentSpeedA = 0;
    _currentSpeedB = 0;
    setMotorA(0);
    setMotorB(0);
}

void MotorController::brake() {
    _currentSpeedA = 0;
    _currentSpeedB = 0;

    // 主動煞車 (兩接口皆為 HIGH 達成電氣短路制動)
    analogWrite(_pinA1A, 255);
    analogWrite(_pinA1B, 255);
    analogWrite(_pinB1A, 255);
    analogWrite(_pinB1B, 255);
}
