/**
 * motor_control.h — Abstracción del motor BTS7960 H-Bridge.
 */
#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include "telemetry_data.h"

namespace Motor {

/** Inicializa pines Enable y canales LEDC PWM. */
void begin();

/** Mueve el motor hacia adelante con el PWM indicado (0-1023). */
void adelante(int pwm);

/** Mueve el motor hacia atrás con el PWM indicado (0-1023). */
void atras(int pwm);

/** Detiene el motor (ambos canales a 0). */
void freno();

/** Retorna la dirección actual del motor. */
MotorDir getDirection();

/** Retorna el PWM actual aplicado. */
int getCurrentPWM();

}  // namespace Motor

#endif // MOTOR_CONTROL_H
