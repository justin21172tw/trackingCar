#include "TargetFollower.h"

#include <math.h>
#include "Config.h"

namespace {
// 對正門檻：車頭或車尾偏離目標方向超出此角度時，優先原地旋轉對正；
// 低於此值才切換為行駛模式，並以差速微調殘餘偏差。
// 可視實際場地調整，建議範圍：20°~45°
constexpr float ALIGN_THRESHOLD_DEG = 40.0f;

constexpr float MAX_DRIVE_TURN_ANGLE_DEG = 100.0f;
constexpr float TURN_ANGLE_GAIN = 1.38f;
constexpr float MIN_INNER_WHEEL_RATIO = 0.42f;
constexpr uint8_t SPIN_SPEED = Config::MotorParams::RATED_MAX_SPEED;
constexpr uint8_t DIAGONAL_OUTER_SPEED = Config::MotorParams::MAX_SPEED;
constexpr uint8_t STRAIGHT_SPEED = static_cast<uint8_t>(
    (Config::MotorParams::DEFAULT_SPEED < Config::MotorParams::RATED_MAX_SPEED
         ? Config::MotorParams::DEFAULT_SPEED
         : Config::MotorParams::RATED_MAX_SPEED) /
    1.25f
);
constexpr uint32_t DRIVE_PULSE_MS = 300;

float computeTurnRatio(float angleOffsetDeg) {
    const float effectiveAngle = angleOffsetDeg * TURN_ANGLE_GAIN;
    return constrain(
        1.0f - effectiveAngle / MAX_DRIVE_TURN_ANGLE_DEG,
        MIN_INNER_WHEEL_RATIO,
        1.0f
    );
}
}

TargetFollower::TargetFollower(MotorController& motor)
    : _motor(motor),
      _pulseStartMs(0),
      _pulseActive(false) {
}

TargetFollower::State TargetFollower::update(
    const TargetSelector::Target& target
) {
    if (!target.found) {
        _motor.stop();
        _pulseActive = false;
        return State::NoTarget;
    }

    const float angleMagnitude = fabsf(target.directionDeg);

    // ── 決定以車頭或車尾朝向目標 ────────────────────────────────────
    // 前半圓（|angle| ≤ 90°）→ 車頭朝目標前進
    // 後半圓（|angle| >  90°）→ 車尾朝目標後退
    const bool useRear = (angleMagnitude > 90.0f);

    // 對正誤差：所選端（前/後）距目標方向的角度差（恆 ≥ 0）
    const float alignError = useRear ? (180.0f - angleMagnitude) : angleMagnitude;

    // ── 旋轉方向 ────────────────────────────────────────────────────
    // 車頭對正：目標偏右(+) → 右旋，目標偏左(-) → 左旋
    // 車尾對正：目標偏右(+) → 左旋，目標偏左(-) → 右旋
    //   理由：車尾在 180° 方向，把車尾轉向右側目標需要整車左旋
    const bool spinRight = useRear
        ? (target.directionDeg < 0.0f)
        : (target.directionDeg > 0.0f);

    // ── 目標過近：退開 ───────────────────────────────────────────────
    if (target.distanceCm < Config::TrackingParams::TARGET_DISTANCE_CM) {
        if (alignError >= ALIGN_THRESHOLD_DEG) {
            // 未對正：旋轉對正優先，避免以斜角衝撞
            spinRight ? _motor.spinRight(SPIN_SPEED) : _motor.spinLeft(SPIN_SPEED);
            _pulseStartMs = millis();
            _pulseActive = true;
            return State::Turning;
        }
        // 已對正且過近：沿對正軸退開
        const uint8_t retreatSpeed = static_cast<uint8_t>(
            STRAIGHT_SPEED * Config::TrackingParams::CLOSE_RETREAT_SPEED_RATIO
        );
        useRear ? _motor.forward(retreatSpeed) : _motor.backward(retreatSpeed);
        _pulseStartMs = millis();
        _pulseActive = true;
        return State::Following;
    }

    // ── 緩衝帶：停車 ─────────────────────────────────────────────────
    if (target.distanceCm <=
        Config::TrackingParams::TARGET_DISTANCE_CM +
            Config::TrackingParams::TARGET_DEADBAND_CM) {
        _motor.stop();
        _pulseActive = false;
        return State::TargetNearby;
    }

    // ── 追蹤：未對正先旋轉，對正後行駛 ──────────────────────────────
    if (alignError >= ALIGN_THRESHOLD_DEG) {
        // 偏差過大：原地旋轉對正
        spinRight ? _motor.spinRight(SPIN_SPEED) : _motor.spinLeft(SPIN_SPEED);
        _pulseStartMs = millis();
        _pulseActive = true;
        return State::Turning;
    }

    // 已對正：行駛並以差速修正殘餘偏差
    const float turnRatio = computeTurnRatio(alignError);
    if (useRear) {
        // 車尾朝目標 → 後退（依目標在左/右微調差速保持對正）
        if (target.directionDeg > 0.0f) {
            _motor.backwardRight(DIAGONAL_OUTER_SPEED, turnRatio);
        } else if (target.directionDeg < 0.0f) {
            _motor.backwardLeft(DIAGONAL_OUTER_SPEED, turnRatio);
        } else {
            _motor.backward(STRAIGHT_SPEED);
        }
    } else {
        // 車頭朝目標 → 前進（依目標在左/右微調差速保持對正）
        if (target.directionDeg > 0.0f) {
            _motor.turnRight(DIAGONAL_OUTER_SPEED, turnRatio);
        } else if (target.directionDeg < 0.0f) {
            _motor.turnLeft(DIAGONAL_OUTER_SPEED, turnRatio);
        } else {
            _motor.forward(STRAIGHT_SPEED);
        }
    }
    _pulseStartMs = millis();
    _pulseActive = true;
    return State::Following;
}

void TargetFollower::maintainPulse() {
    if (_pulseActive && (millis() - _pulseStartMs >= DRIVE_PULSE_MS)) {
        _motor.stop();
        _pulseActive = false;
    }
}

const char* TargetFollower::stateName(State state) {
    switch (state) {
        case State::NoTarget:
            return "no_target";
        case State::TargetNearby:
            return "target_nearby";
        case State::Following:
            return "following";
        case State::Turning:
            return "turning";
    }
    return "no_target";
}
