/**
 * motor_control.cpp — Implementación del control de motor BTS7960.
 */
#include "motor_control.h"
#include "hardware_pins.h"

namespace Motor {

static MotorDir _direction = DIR_STOP;
static int      _currentPWM = 0;

void begin() {
    pinMode(PIN_R_EN, OUTPUT);
    pinMode(PIN_L_EN, OUTPUT);
    digitalWrite(PIN_R_EN, HIGH);
    digitalWrite(PIN_L_EN, HIGH);

    ledcSetup(LEDC_CH_R, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(LEDC_CH_L, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_R_PWM, LEDC_CH_R);
    ledcAttachPin(PIN_L_PWM, LEDC_CH_L);

    freno();
}

void adelante(int pwm) {
    _direction  = DIR_ADELANTE;
    _currentPWM = pwm;
    ledcWrite(LEDC_CH_L, 0);
    ledcWrite(LEDC_CH_R, pwm);
}

void atras(int pwm) {
    _direction  = DIR_ATRAS;
    _currentPWM = pwm;
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, pwm);
}

void freno() {
    _direction  = DIR_STOP;
    _currentPWM = 0;
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, 0);
}

MotorDir getDirection() { return _direction; }
int getCurrentPWM()     { return _currentPWM; }

}  // namespace Motor
