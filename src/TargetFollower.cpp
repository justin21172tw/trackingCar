#include "TargetFollower.h"

#include <math.h>
#include "Config.h"

namespace {
constexpr float MAX_DRIVE_TURN_ANGLE_DEG = 100.0f;
constexpr float TURN_ANGLE_GAIN = 3.8f;
constexpr float MIN_INNER_WHEEL_RATIO = -0.60f;
constexpr float SIDE_SPIN_MIN_ANGLE_DEG = 85.0f;
constexpr float REAR_TRACK_MIN_ANGLE_DEG = 120.0f;
constexpr float REAR_STRAIGHT_MIN_ANGLE_DEG = 175.0f;
constexpr uint8_t SPIN_SPEED = Config::MotorParams::DEFAULT_SPEED;
constexpr uint8_t STRAIGHT_SPEED = static_cast<uint8_t>(
    (Config::MotorParams::DEFAULT_SPEED < Config::MotorParams::MAX_SPEED
         ? Config::MotorParams::DEFAULT_SPEED
         : Config::MotorParams::MAX_SPEED) /
    1.25f
);
constexpr uint32_t DRIVE_PULSE_MS = 180;

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

    if (target.distanceCm <= Config::TrackingParams::STOP_DISTANCE_CM) {
        _motor.stop();
        _pulseActive = false;
        return State::TargetNearby;
    }

    const float angleMagnitude = fabsf(target.directionDeg);
    const uint8_t speed = Config::MotorParams::DEFAULT_SPEED;

    if (angleMagnitude >= REAR_TRACK_MIN_ANGLE_DEG) {
        if (angleMagnitude >= REAR_STRAIGHT_MIN_ANGLE_DEG) {
            _motor.backward(STRAIGHT_SPEED);
        } else {
            const float rearOffsetDeg = 180.0f - angleMagnitude;
            const float turnRatio = computeTurnRatio(rearOffsetDeg);
            if (target.directionDeg > 0.0f) {
                _motor.backwardRight(speed, turnRatio);
            } else {
                _motor.backwardLeft(speed, turnRatio);
            }
        }
        _pulseStartMs = millis();
        _pulseActive = true;
        return State::Following;
    }

    if (angleMagnitude >= SIDE_SPIN_MIN_ANGLE_DEG) {
        if (target.directionDeg > 0.0f) {
            _motor.spinRight(SPIN_SPEED);
        } else {
            _motor.spinLeft(SPIN_SPEED);
        }
        _pulseStartMs = millis();
        _pulseActive = true;
        return State::Turning;
    }

    const float turnRatio = computeTurnRatio(angleMagnitude);
    if (target.directionDeg > 0.0f) {
        _motor.turnRight(speed, turnRatio);
    } else if (target.directionDeg < 0.0f) {
        _motor.turnLeft(speed, turnRatio);
    } else {
        _motor.forward(STRAIGHT_SPEED);
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
