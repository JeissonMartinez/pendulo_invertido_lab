#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

enum Direction {
    DIR_STOP,
    DIR_FORWARD,
    DIR_REVERSE
};

void      motor_init();
void      motor_setSpeed(uint8_t pwm);
void      motor_forward();
void      motor_reverse();
void      motor_stop();
void      motor_brake();
uint8_t   motor_getSpeed();
Direction motor_getDirection();
const char* motor_getDirectionStr();

#endif // MOTOR_H
