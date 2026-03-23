#include "motor.h"
#include "config.h"

#define LEDC_CHANNEL 0  // Canal LEDC para PWM del motor

static Direction currentDirection = DIR_STOP;
static uint8_t   currentSpeed    = 0;

void motor_init() {
    pinMode(PIN_STBY, OUTPUT);
    pinMode(PIN_BIN1, OUTPUT);
    pinMode(PIN_BIN2, OUTPUT);

    digitalWrite(PIN_STBY, HIGH);  // Activar driver
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, LOW);

    ledcSetup(LEDC_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_PWMB, LEDC_CHANNEL);
    ledcWrite(LEDC_CHANNEL, 0);
}

void motor_setSpeed(uint8_t pwm) {
    currentSpeed = pwm;
    ledcWrite(LEDC_CHANNEL, pwm);
}

void motor_forward() {
    currentDirection = DIR_FORWARD;
    digitalWrite(PIN_BIN1, HIGH);
    digitalWrite(PIN_BIN2, LOW);
}

void motor_reverse() {
    currentDirection = DIR_REVERSE;
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, HIGH);
}

void motor_stop() {
    currentDirection = DIR_STOP;
    currentSpeed = 0;
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, LOW);
    ledcWrite(LEDC_CHANNEL, 0);
}

void motor_brake() {
    currentDirection = DIR_STOP;
    currentSpeed = 0;
    digitalWrite(PIN_BIN1, HIGH);
    digitalWrite(PIN_BIN2, HIGH);
    ledcWrite(LEDC_CHANNEL, 0);
}

uint8_t motor_getSpeed() {
    return currentSpeed;
}

Direction motor_getDirection() {
    return currentDirection;
}

const char* motor_getDirectionStr() {
    switch (currentDirection) {
        case DIR_FORWARD: return "FORWARD";
        case DIR_REVERSE: return "REVERSE";
        default:          return "STOP";
    }
}
