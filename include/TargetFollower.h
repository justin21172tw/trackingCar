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
    static const char* stateName(State state);

private:
    MotorController& _motor;
};
