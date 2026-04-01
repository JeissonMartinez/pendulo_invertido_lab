/**
 * encoder_ir.h — Encoder infrarrojo HC-020K (motor).
 * ISR, conteo de pulsos, cálculo RPM/RPS/vueltas/posición.
 */
#ifndef ENCODER_IR_H
#define ENCODER_IR_H

#include <Arduino.h>
#include "telemetry_data.h"

namespace EncoderIR {

/** Inicializa el pin y la interrupción del encoder IR. */
void begin();

/** Lee el conteo actual de pulsos de forma atómica. */
uint32_t getPulseCount();

/** Resetea el conteo de pulsos a cero. */
void resetPulseCount();

/** Lee la posición global (con signo) de forma atómica. */
int32_t getGlobalPosition();

/** Resetea la posición global a cero. */
void resetGlobalPosition();

/** Setea la posición global a un valor específico en cm. */
void setPositionCm(float cm);

/**
 * Calcula RPM/RPS instantáneos basado en pulsos acumulados.
 * Llamar periódicamente (~5-10 Hz) para obtener valores precisos.
 */
void updateRPM();

/** Últimos valores calculados. */
float getRPS();
float getRPM();
float getVueltas();
float getPositionCm();

/** Modo debug — imprime pulsos por serial cada 100ms */
void enableDebug();
void disableDebug();
bool isDebugEnabled();

}  // namespace EncoderIR

#endif // ENCODER_IR_H
