#include "TargetFollower.h"

#include <math.h>
#include "Config.h"

namespace {
constexpr float MAX_DRIVE_TURN_ANGLE_DEG = 100.0f;
constexpr float MIN_INNER_WHEEL_RATIO = 0.25f;
constexpr float SPIN_ANGLE_DEG = 100.0f;
constexpr uint8_t SPIN_SPEED = 130;
}

TargetFollower::TargetFollower(MotorController& motor)
    : _motor(motor) {
}

TargetFollower::State TargetFollower::update(
    const TargetSelector::Target& target
) {
    if (!target.found) {
        _motor.stop();
        return State::NoTarget;
    }

    if (target.distanceCm <= Config::TrackingParams::STOP_DISTANCE_CM) {
        _motor.stop();
        return State::TargetNearby;
    }

    if (target.directionDeg >= SPIN_ANGLE_DEG) {
        _motor.spinRight(SPIN_SPEED);
        return State::Turning;
    }
    if (target.directionDeg <= -SPIN_ANGLE_DEG) {
        _motor.spinLeft(SPIN_SPEED);
        return State::Turning;
    }

    const float angleMagnitude = fabsf(target.directionDeg);
    const float turnRatio = constrain(
        1.0f - angleMagnitude / MAX_DRIVE_TURN_ANGLE_DEG,
        MIN_INNER_WHEEL_RATIO,
        1.0f
    );
    const uint8_t speed = Config::MotorParams::DEFAULT_SPEED;

    if (target.directionDeg > 0.0f) {
        _motor.turnRight(speed, turnRatio);
    } else if (target.directionDeg < 0.0f) {
        _motor.turnLeft(speed, turnRatio);
    } else {
        _motor.forward(speed);
    }
    return State::Following;
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
