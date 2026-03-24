/**
 * safety.h — Módulo de seguridad del sistema.
 * Emergency stop, verificaciones de límites, timeout WiFi.
 */
#ifndef SAFETY_H
#define SAFETY_H

#include <Arduino.h>

namespace Safety {

/** Ejecuta freno de emergencia inmediato. */
void emergencyStop();

/**
 * Verifica condiciones de seguridad. Llamar en cada loop.
 * Frena el motor si se detecta una condición peligrosa.
 * @return true si todo OK, false si se activó freno de seguridad.
 */
bool check();

/** Registra timestamp del último dato MQTT recibido. */
void markMqttActivity();

/** Timeout MQTT: si no recibimos datos en N ms, es peligroso. */
void setMqttTimeout(unsigned long timeoutMs);

}  // namespace Safety

#endif // SAFETY_H
