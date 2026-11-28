#pragma once

#include <Arduino.h>
#include "MotorController.h"
#include "TargetSelector.h"

class TargetFollower {
public:
    enum class State : uint8_t {
        NoTarget,
        TargetNearby,
        Following,
        Turning
    };

    explicit TargetFollower(MotorController& motor);

    State update(const TargetSelector::Target& target);
    void maintainPulse();
    static const char* stateName(State state);

private:
    MotorController& _motor;
    uint32_t _pulseStartMs;
    bool _pulseActive;
};
