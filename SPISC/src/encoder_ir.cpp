/**
 * encoder_ir.cpp — Implementación del encoder infrarrojo HC-020K.
 * 
 * Usa el periférico PCNT (Pulse Counter) del ESP32 para conteo
 * por hardware, eliminando pérdida de pulsos por latencia de ISR.
 */
#include "encoder_ir.h"
#include "hardware_pins.h"
#include "motor_control.h"
#include "sensors_i2c.h"
#include <driver/pcnt.h>

namespace EncoderIR {

// ══════════════════════════════════════════════════
// Variables PCNT
// ══════════════════════════════════════════════════
static volatile int32_t _overflowCount = 0;  // Acumulador de overflows (±10000)
static int32_t _globalPosition = 0;          // Posición con signo
static int16_t _lastPcntValue = 0;           // Último valor leído de PCNT

// Valores calculados
static float _rps     = 0;
static float _rpm     = 0;
static float _vueltas = 0;
static float _posCm   = 0;

// Para cálculo de RPM
static uint32_t      _prevPulses = 0;
static unsigned long _prevTime   = 0;

// Modo debug
static bool _debugMode = false;

// ══════════════════════════════════════════════════
// ISR para overflow/underflow del contador PCNT
// ══════════════════════════════════════════════════
static void IRAM_ATTR _pcntOverflowISR(void* arg) {
    uint32_t status = 0;
    pcnt_get_event_status(PCNT_UNIT_ENCODER, &status);
    
    if (status & PCNT_EVT_H_LIM) {
        _overflowCount++;
    }
    if (status & PCNT_EVT_L_LIM) {
        _overflowCount--;
    }
}

// ══════════════════════════════════════════════════
// Funciones internas
// ══════════════════════════════════════════════════

/**
 * Lee el valor raw del PCNT incluyendo overflows.
 * Retorna el conteo total de pulsos (siempre positivo).
 */
static uint32_t _getRawPulseCount() {
    int16_t count = 0;
    pcnt_get_counter_value(PCNT_UNIT_ENCODER, &count);
    
    // Calcular total con overflows
    int32_t total = _overflowCount * PCNT_H_LIM + count;
    return (total >= 0) ? (uint32_t)total : 0;
}

/**
 * Actualiza la posición global aplicando dirección del sensor magnético.
 * Debe llamarse periódicamente (en updateRPM o desde el loop).
 * Usa la velocidad del AS5600 para detectar dirección, permitiendo
 * detectar movimientos manuales del carro (no solo por motor).
 */
static void _updateGlobalPosition() {
    int16_t currentPcnt = 0;
    pcnt_get_counter_value(PCNT_UNIT_ENCODER, &currentPcnt);
    
    // Calcular delta desde última lectura
    int16_t delta = currentPcnt - _lastPcntValue;
    
    // Manejar wraparound del contador PCNT
    if (delta > PCNT_H_LIM / 2) {
        delta -= PCNT_H_LIM;  // Underflow ocurrió
    } else if (delta < -PCNT_H_LIM / 2) {
        delta += PCNT_H_LIM;  // Overflow ocurrió
    }
    
    // Aplicar dirección basada en sensor magnético (detecta mov. manuales)
    if (delta != 0) {
        int absDelta = (delta > 0) ? delta : -delta;
        
        if (_debugMode) {
            // En modo debug, acumular siempre positivo
            _globalPosition += absDelta;
        } else {
            // Usar velocidad del sensor magnético para determinar dirección
            float velMag = SensorsI2C::getMagVelocityCmPerSec();
            const float VEL_THRESHOLD = 0.5f;  // cm/s umbral para considerar movimiento
            
            if (velMag > VEL_THRESHOLD) {
                // Movimiento hacia adelante (posición aumenta)
                _globalPosition += absDelta;
            } else if (velMag < -VEL_THRESHOLD) {
                // Movimiento hacia atrás (posición disminuye)
                _globalPosition -= absDelta;
            }
            // Si |velMag| < umbral, ignorar pulsos (ruido o carro quieto)
        }
    }
    
    _lastPcntValue = currentPcnt;
}

// ══════════════════════════════════════════════════
// API pública
// ══════════════════════════════════════════════════

void begin() {
    // Configuración del canal PCNT
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = PIN_ENCODER_IR,
        .ctrl_gpio_num  = PCNT_PIN_NOT_USED,
        .lctrl_mode     = PCNT_MODE_KEEP,
        .hctrl_mode     = PCNT_MODE_KEEP,
        .pos_mode       = PCNT_COUNT_INC,    // Incrementa en flanco positivo
        .neg_mode       = PCNT_COUNT_DIS,    // Ignora flanco negativo
        .counter_h_lim  = PCNT_H_LIM,
        .counter_l_lim  = PCNT_L_LIM,
        .unit           = PCNT_UNIT_ENCODER,
        .channel        = PCNT_CHANNEL_ENC,
    };
    pcnt_unit_config(&pcnt_config);
    
    // Filtro anti-glitch (elimina pulsos < ~1.25µs)
    pcnt_set_filter_value(PCNT_UNIT_ENCODER, PCNT_FILTER_VAL);
    pcnt_filter_enable(PCNT_UNIT_ENCODER);
    
    // Configurar eventos de overflow/underflow
    pcnt_event_enable(PCNT_UNIT_ENCODER, PCNT_EVT_H_LIM);
    pcnt_event_enable(PCNT_UNIT_ENCODER, PCNT_EVT_L_LIM);
    
    // Instalar servicio de ISR y registrar handler
    pcnt_isr_service_install(0);
    pcnt_isr_handler_add(PCNT_UNIT_ENCODER, _pcntOverflowISR, NULL);
    
    // Inicializar contador
    pcnt_counter_pause(PCNT_UNIT_ENCODER);
    pcnt_counter_clear(PCNT_UNIT_ENCODER);
    pcnt_counter_resume(PCNT_UNIT_ENCODER);
    
    _overflowCount = 0;
    _globalPosition = 0;
    _lastPcntValue = 0;
    _prevTime = millis();
    
    Serial.println("[EncoderIR] PCNT inicializado en GPIO " + String(PIN_ENCODER_IR));
}

uint32_t getPulseCount() {
    return _getRawPulseCount();
}

void resetPulseCount() {
    pcnt_counter_pause(PCNT_UNIT_ENCODER);
    pcnt_counter_clear(PCNT_UNIT_ENCODER);
    _overflowCount = 0;
    _lastPcntValue = 0;
    pcnt_counter_resume(PCNT_UNIT_ENCODER);
    
    _prevPulses = 0;
    _prevTime = millis();
}

int32_t getGlobalPosition() {
    return _globalPosition;
}

void resetGlobalPosition() {
    _globalPosition = 0;
}

void setPositionCm(float cm) {
    // Convertir cm a pulsos y setear posición global
    _globalPosition = (int32_t)(cm / CM_POR_PULSO);
    Serial.printf("[EncoderIR] Posición calibrada a %.1f cm (%ld pulsos)\n", cm, _globalPosition);
}

void updateRPM() {
    // Primero actualizar posición global
    _updateGlobalPosition();
    
    uint32_t pulses = getPulseCount();
    unsigned long now = millis();
    unsigned long dt = now - _prevTime;

    if (_prevTime > 0 && dt > 0) {
        uint32_t dp = (pulses >= _prevPulses) ? (pulses - _prevPulses) : 0;
        float dtSec = dt / 1000.0f;
        _rps = (float)dp / dtSec / PULSES_PER_REV;
        _rpm = _rps * 60.0f;
    }

    _prevPulses = pulses;
    _prevTime   = now;
    _vueltas    = (float)pulses / PULSES_PER_REV;
    _posCm      = _globalPosition * CM_POR_PULSO;

    // Debug: imprimir cada 100ms si está habilitado
    static unsigned long lastDebugPrint = 0;
    if (_debugMode && (now - lastDebugPrint >= 100)) {
        lastDebugPrint = now;
        int16_t rawPcnt = 0;
        pcnt_get_counter_value(PCNT_UNIT_ENCODER, &rawPcnt);
        Serial.printf("[ENCODER_DEBUG] pulsos=%lu, globalPos=%ld, rawPCNT=%d, overflows=%ld, posCm=%.2f, rpm=%.1f\n",
                      pulses, _globalPosition, rawPcnt, _overflowCount, _posCm, _rpm);
    }
}

float getRPS()        { return _rps; }
float getRPM()        { return _rpm; }
float getVueltas()    { return _vueltas; }
float getPositionCm() { return _posCm; }

// ══════════════════════════════════════════════════
// Modo Debug
// ══════════════════════════════════════════════════

void enableDebug() {
    _debugMode = true;
    Serial.println("[EncoderIR] === DEBUG MODE ENABLED ===");
    Serial.println("[EncoderIR] Mueve el carro manualmente y observa los pulsos.");
    Serial.println("[EncoderIR] Envía {\"encoder\":\"off\"} para desactivar.");
    resetPulseCount();
    resetGlobalPosition();
}

void disableDebug() {
    _debugMode = false;
    uint32_t finalPulses = getPulseCount();
    float finalCm = _globalPosition * CM_POR_PULSO;
    Serial.println("[EncoderIR] === DEBUG MODE DISABLED ===");
    Serial.printf("[EncoderIR] Total pulsos: %lu\n", finalPulses);
    Serial.printf("[EncoderIR] Posición final: %.2f cm\n", finalCm);
    Serial.printf("[EncoderIR] Si recorriste X cm, CM_POR_PULSO = X / %lu\n", finalPulses);
}

bool isDebugEnabled() { return _debugMode; }

}  // namespace EncoderIR
