/**
 * pid_cascada_mode.h — Modo Control PID en Cascada.
 * 
 * Control PID cascada: lazo externo (posición) genera θ_ref,
 * lazo interno (ángulo) estabiliza el péndulo usando θ_ref como setpoint.
 * 
 * Convención angular: 0° = péndulo arriba (vertical), 180° = abajo.
 * La salida del lazo externo se satura a ±theta_ref_max (default ±2°).
 * 
 * Estados:
 *   PIDC_IDLE → PIDC_HOMING_START → PIDC_CALIBRATING → PIDC_WAIT_READY
 *     → PIDC_READY → PIDC_RUNNING → PIDC_STOPPED
 * 
 * Comandos MQTT (via pendulum/cmd/pidcasc):
 *   {"action":"home"}       — mover a LS_START
 *   {"action":"end"}        — mover a LS_END
 *   {"action":"config",...}  — configurar parámetros de ambos lazos
 *   {"action":"run"}        — iniciar secuencia (homing→calib→wait→ready)
 *   {"action":"stop"}       — detener inmediatamente
 *   {"action":"reset"}      — volver a IDLE
 */
#ifndef PID_CASCADA_MODE_H
#define PID_CASCADA_MODE_H

#include <Arduino.h>

namespace PidCascadaMode {

// ══════════════════════════════════════════════════
// API Pública — Interfaz de modo
// ══════════════════════════════════════════════════

/** Inicializa/resetea el modo. Auto-inicia homing a LS_START. */
void begin();

/** Loop principal del modo (ejecutado a 1000 Hz en Core 0). */
void loop();

/** Procesa comandos MQTT para este modo. */
void onCommand(const char* topic, const char* payload);

/** Retorna el estado actual como string ("PIDC_READY", "PIDC_RUNNING", etc). */
const char* getStateStr();

/** ¿Está en un estado activo (homing, controlando)? */
bool isActive();

// ══════════════════════════════════════════════════
// Getters para Telemetría — Lazo externo (Posición)
// ══════════════════════════════════════════════════

float getPosError();
float getPosIntegral();
float getPosDerivada();
float getPosOutput();
float getPosOutputSaturated();
float getPosSetpoint();
float getPosKp();
float getPosKi();
float getPosKd();
float getPosZonaSeguridad();

// ══════════════════════════════════════════════════
// Getters para Telemetría — Lazo interno (Ángulo)
// ══════════════════════════════════════════════════

float getAngError();
float getAngIntegral();
float getAngDerivada();
float getAngOutput();
float getAngOutputSaturated();
float getAngSetpoint();       // θ_ref (variable, viene del lazo externo)
float getAngKp();
float getAngKi();
float getAngKd();

// ══════════════════════════════════════════════════
// Getters para Telemetría — Parámetros comunes
// ══════════════════════════════════════════════════

float getRangoTrabajo();
float getToleranciaAng();
int   getPwmMin();
int   getPwmMax();
float getThetaRefMax();

}

#endif // PID_CASCADA_MODE_H
