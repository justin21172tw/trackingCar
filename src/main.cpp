#include <Arduino.h>
#include <math.h>
#include "Config.h"
#include "MotorController.h"
#include "UltrasonicArray.h"

namespace {

MotorController motor(
    Config::MotorPins::PIN_A1A,
    Config::MotorPins::PIN_A1B,
    Config::MotorPins::PIN_B1A,
    Config::MotorPins::PIN_B1B
);
UltrasonicArray ultrasonicArray;
bool ultrasonicArrayReady = false;

void followTarget(uint8_t sensorIndex, float distanceCm) {
    if (distanceCm < 0.0f ||
        distanceCm > Config::TrackingParams::MAX_TRACK_DISTANCE_CM) {
        motor.stop();
        Serial.printf(
            "event=tracking status=target_lost sensor=%u distance_cm=%.1f motor_left=%d motor_right=%d\n",
            sensorIndex,
            distanceCm,
            motor.getCurrentLeftSpeed(),
            motor.getCurrentRightSpeed()
        );
        return;
    }

    if (distanceCm < Config::TrackingParams::STOP_DISTANCE_CM) {
        motor.brake();
        Serial.printf(
            "event=tracking status=too_close sensor=%u distance_cm=%.1f motor_left=%d motor_right=%d\n",
            sensorIndex,
            distanceCm,
            motor.getCurrentLeftSpeed(),
            motor.getCurrentRightSpeed()
        );
        return;
    }

    const float targetAngle = ultrasonicArray.getSensorAngle(sensorIndex);
    const int baseSpeed =
        distanceCm <= Config::TrackingParams::TARGET_DISTANCE_CM + 5.0f
            ? 120
            : Config::MotorParams::DEFAULT_SPEED;
    const float steering = constrain(targetAngle / 70.0f, -1.0f, 1.0f);
    const int adjustment =
        static_cast<int>(baseSpeed * 0.75f * fabsf(steering));

    int leftSpeed = baseSpeed;
    int rightSpeed = baseSpeed;
    if (steering < 0.0f) {
        leftSpeed -= adjustment;
        rightSpeed += adjustment;
    } else {
        leftSpeed += adjustment;
        rightSpeed -= adjustment;
    }
    motor.drive(leftSpeed, rightSpeed);

    Serial.printf(
        "event=tracking status=following sensor=%u echo_gpio=%u angle_deg=%.1f distance_cm=%.1f motor_left=%d motor_right=%d\n",
        sensorIndex,
        ultrasonicArray.getEchoPin(sensorIndex),
        targetAngle,
        distanceCm,
        motor.getCurrentLeftSpeed(),
        motor.getCurrentRightSpeed()
    );
}

}

void setup() {
    Serial.begin(Config::SERIAL_BAUD_RATE);
    motor.init();
    motor.setTrim(
        Config::MotorParams::DEFAULT_TRIM_LEFT,
        Config::MotorParams::DEFAULT_TRIM_RIGHT
    );

    ultrasonicArrayReady = ultrasonicArray.init();
    if (!ultrasonicArrayReady) {
        motor.stop();
        Serial.println("event=setup level=error component=ultrasonic_array message=mcp23017_init_failed");
        return;
    }

    Serial.printf(
        "event=setup level=info sensor_count=%u echo_pins=26,27,32,33,34,35,36,39\n",
        ultrasonicArray.getSensorCount()
    );
}

void loop() {
    if (!ultrasonicArrayReady) {
        motor.stop();
        delay(1000);
        return;
    }

    int targetSensor = -1;
    float targetDistance = -1.0f;
    float nearestDistance = Config::TrackingParams::MAX_TRACK_DISTANCE_CM;

    for (uint8_t sensorIndex = 0;
         sensorIndex < ultrasonicArray.getSensorCount();
         ++sensorIndex) {
        const float distanceCm = ultrasonicArray.measureDistance(sensorIndex);
        if (distanceCm > 0.0f && distanceCm <= nearestDistance) {
            nearestDistance = distanceCm;
            targetDistance = distanceCm;
            targetSensor = sensorIndex;
        }
    }

    if (targetSensor < 0) {
        motor.stop();
        Serial.println("event=tracking level=info status=no_target motor_left=0 motor_right=0");
        return;
    }

    followTarget(static_cast<uint8_t>(targetSensor), targetDistance);
}
