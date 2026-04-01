/**
 * pid_posicion_mode.h — Modo Control PID de Posición.
 * 
 * Control PID para posicionamiento del carro en el riel.
 * Incluye zonas de seguridad configurables y telemetría extendida
 * con error, integral y derivada para visualización en HMI.
 * 
 * Estados:
 *   PID_IDLE → PID_HOMING_START → PID_READY → PID_RUNNING → PID_AT_TARGET
 *                                                        → PID_STOPPED
 * 
 * Comandos MQTT (via pendulum/cmd/pidpos):
 *   {"action":"home"}                     — mover a LS_START
 *   {"action":"end"}                      — mover a LS_END
 *   {"action":"config","kp":...,"ki":...,"kd":...,"setpoint":...,"zona_seguridad":...}
 *   {"action":"run"}                      — iniciar control PID
 *   {"action":"stop"}                     — detener inmediatamente
 *   {"action":"reset"}                    — volver a IDLE
 */
#ifndef PID_POSICION_MODE_H
#define PID_POSICION_MODE_H

#include <Arduino.h>

namespace PidPosicionMode {

// ══════════════════════════════════════════════════
// API Pública — Interfaz de modo
// ══════════════════════════════════════════════════

/** Inicializa/resetea el modo. Auto-inicia homing a LS_START. */
void begin();

/** Loop principal del modo (ejecutado a 500-1000 Hz en Core 0). */
void loop();

/** Procesa comandos MQTT para este modo. */
void onCommand(const char* topic, const char* payload);

/** Retorna el estado actual como string ("PID_READY", "PID_RUNNING", etc). */
const char* getStateStr();

/** ¿Está en un estado activo (homing o controlando)? */
bool isActive();

// ══════════════════════════════════════════════════
// Getters para Telemetría PID
// ══════════════════════════════════════════════════

/** Error actual: setpoint - posición_actual (cm) */
float getError();

/** Término integral acumulado */
float getIntegral();

/** Término derivativo (derivada del error) */
float getDerivada();

/** Salida del controlador PID (PWM antes de saturación) */
float getOutput();

/** Setpoint configurado (cm) */
float getSetpoint();

/** Constantes PID actuales */
float getKp();
float getKi();
float getKd();

/** Zona de seguridad configurada (cm desde cada extremo) */
float getZonaSeguridad();

/** Tolerancia para considerar "llegada" al setpoint (cm) */
float getTolerancia();

}  // namespace PidPosicionMode

#endif // PID_POSICION_MODE_H
