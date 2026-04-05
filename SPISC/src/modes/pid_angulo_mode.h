/**
 * pid_angulo_mode.h — Modo Control PID de Ángulo.
 * 
 * Control PID para estabilización angular del péndulo invertido.
 * Incluye pre-posicionamiento del carro a 45 cm, detección de rango
 * de trabajo configurable y re-posicionamiento automático al salir.
 * 
 * Convención angular: 0° = péndulo arriba (vertical), 180° = abajo.
 * El setpoint siempre es 0°/360° (vertical hacia arriba).
 * 
 * Estados:
 *   PIDA_IDLE → PIDA_HOMING_START → PIDA_CALIBRATING → PIDA_PREPOSITIONING
 *     → PIDA_READY → PIDA_RUNNING → PIDA_REPOSITIONING → PIDA_READY
 *                                 → PIDA_STOPPED
 * 
 * Comandos MQTT (via pendulum/cmd/pidang):
 *   {"action":"home"}                     — mover a LS_START
 *   {"action":"end"}                      — mover a LS_END
 *   {"action":"config","kp":...,"ki":...,"kd":...,"rango_trabajo":...}
 *   {"action":"run"}                      — iniciar secuencia completa
 *   {"action":"stop"}                     — detener inmediatamente
 *   {"action":"reset"}                    — volver a IDLE
 */
#ifndef PID_ANGULO_MODE_H
#define PID_ANGULO_MODE_H

#include <Arduino.h>

namespace PidAnguloMode {

// ══════════════════════════════════════════════════
// API Pública — Interfaz de modo
// ══════════════════════════════════════════════════

/** Inicializa/resetea el modo. Auto-inicia homing a LS_START. */
void begin();

/** Loop principal del modo (ejecutado a 1000 Hz en Core 0). */
void loop();

/** Procesa comandos MQTT para este modo. */
void onCommand(const char* topic, const char* payload);

/** Retorna el estado actual como string ("PIDA_READY", "PIDA_RUNNING", etc). */
const char* getStateStr();

/** ¿Está en un estado activo (homing, pre-posicionando, controlando o reposicionando)? */
bool isActive();

// ══════════════════════════════════════════════════
// Getters para Telemetría PID Ángulo
// ══════════════════════════════════════════════════

/** Error angular actual (grados, normalizado ±180) */
float getError();

/** Término integral acumulado */
float getIntegral();

/** Término derivativo (filtrado) */
float getDerivada();

/** Salida PID cruda (antes de saturación) */
float getOutput();

/** Salida PID saturada (PWM real aplicado con signo) */
float getOutputSaturated();

/** Setpoint angular (siempre 0°) */
float getSetpoint();

/** Constantes PID actuales */
float getKp();
float getKi();
float getKd();

/** Rango de trabajo configurado (grados desde vertical) */
float getRangoTrabajo();

/** Tolerancia angular (grados) */
float getTolerancia();

/** PWM mínimo configurado */
int getPwmMin();

/** PWM máximo configurado */
int getPwmMax();

/** Setpoint de posición para pre-posicionamiento (cm) */
float getPosSetpoint();

// ══════════════════════════════════════════════════
// Getters para Telemetría PID Posición interno
// ══════════════════════════════════════════════════

/** Error de posición interno (cm) */
float getPosError();

/** Integral de posición interno */
float getPosIntegral();

/** Derivada de posición interno (filtrada) */
float getPosDerivada();

/** Salida PID de posición interno */
float getPosOutput();

}  // namespace PidAnguloMode

#endif // PID_ANGULO_MODE_H
