/**
 * rango_pwm_mode.h — Modo Barrido PWM (extraído de rango_pwm.cpp).
 */
#ifndef RANGO_PWM_MODE_H
#define RANGO_PWM_MODE_H

#include <Arduino.h>

namespace RangoPwmMode {

/** Inicializa el modo (resetea variables de barrido). */
void begin();

/** Loop principal del modo. Llamar en cada iteración. */
void loop();

/** Procesa comandos MQTT para este modo. */
void onCommand(const char* topic, const char* payload);

/** Retorna el nombre del estado actual. */
const char* getStateStr();

/** Indica si el modo está en un estado activo (no IDLE/WAIT). */
bool isActive();

}  // namespace RangoPwmMode

#endif // RANGO_PWM_MODE_H
