/**
 * encoder_ir.cpp — Implementación del encoder infrarrojo HC-020K.
 */
#include "encoder_ir.h"
#include "hardware_pins.h"
#include "motor_control.h"

namespace EncoderIR {

// Variables ISR (volatile)
static volatile uint32_t _isrPulseCount = 0;
static volatile int32_t  _globalPosition = 0;

// Valores calculados
static float _rps     = 0;
static float _rpm     = 0;
static float _vueltas = 0;
static float _posCm   = 0;

// Para cálculo de RPM
static uint32_t      _prevPulses = 0;
static unsigned long _prevTime   = 0;

// ISR del encoder
static void IRAM_ATTR _encoderISR() {
    _isrPulseCount++;
    MotorDir dir = Motor::getDirection();
    if (dir == DIR_ADELANTE)
        _globalPosition++;
    else if (dir == DIR_ATRAS)
        _globalPosition--;
}

void begin() {
    pinMode(PIN_ENCODER_IR, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_IR), _encoderISR, RISING);
    _prevTime = millis();
}

uint32_t getPulseCount() {
    noInterrupts();
    uint32_t c = _isrPulseCount;
    interrupts();
    return c;
}

void resetPulseCount() {
    noInterrupts();
    _isrPulseCount = 0;
    interrupts();
    _prevPulses = 0;
    _prevTime   = millis();
}

int32_t getGlobalPosition() {
    noInterrupts();
    int32_t p = _globalPosition;
    interrupts();
    return p;
}

void resetGlobalPosition() {
    noInterrupts();
    _globalPosition = 0;
    interrupts();
}

void updateRPM() {
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
    _posCm      = getGlobalPosition() * CM_POR_PULSO;
}

float getRPS()        { return _rps; }
float getRPM()        { return _rpm; }
float getVueltas()    { return _vueltas; }
float getPositionCm() { return _posCm; }

}  // namespace EncoderIR
