/**
 * lazo_abierto_mode.h — Modo Lazo Abierto (Open Loop).
 * Permite enviar impulsos individuales de PWM con duración definida.
 */
#ifndef LAZO_ABIERTO_MODE_H
#define LAZO_ABIERTO_MODE_H

#include <Arduino.h>

namespace LazoAbiertoMode {

/** Inicializa/resetea el modo. */
void begin();

/** Loop principal del modo. */
void loop();

/** Procesa comandos MQTT para este modo. */
void onCommand(const char* topic, const char* payload);

/** Retorna el estado actual como string. */
const char* getStateStr();

/** ¿Está en un estado activo? */
bool isActive();

}  // namespace LazoAbiertoMode

#endif // LAZO_ABIERTO_MODE_H
