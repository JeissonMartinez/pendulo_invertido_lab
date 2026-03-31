/**
 * safety.h — Módulo de seguridad del sistema.
 * Emergency stop, verificaciones de límites, timeout WiFi.
 * 
 * Arquitectura Dual-Core:
 * - Watchdog hardware como backup para la tarea de sensores
 * - Si Core 0 se bloquea, el watchdog resetea el ESP32
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

// ══════════════════════════════════════════════════
// Watchdog Hardware (Dual-Core)
// ══════════════════════════════════════════════════

/**
 * Inicializar watchdog de hardware.
 * Si la tarea de sensores no llama a feedWatchdog() en timeoutSeconds,
 * el ESP32 se resetea automáticamente.
 * @param timeoutSeconds Timeout en segundos (recomendado: 5-10s)
 */
void initHardwareWatchdog(uint32_t timeoutSeconds);

/**
 * Alimentar watchdog. Llamar al final de cada iteración de la tarea de sensores.
 * Si no se llama dentro del timeout, el ESP32 se resetea.
 */
void feedWatchdog();

}  // namespace Safety

#endif // SAFETY_H
