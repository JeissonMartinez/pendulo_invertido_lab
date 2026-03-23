#include "encoder.h"
#include "config.h"

static volatile uint32_t isrPulseCount = 0;   // Contador en ISR (volatile)

static uint32_t totalPulses   = 0;  // Pulsos acumulados totales
static uint32_t lastPulses    = 0;  // Pulsos al inicio de la ventana
static unsigned long lastCalcTime = 0;

static float rps = 0.0f;
static float rpm = 0.0f;

// ISR - se ejecuta en cada flanco RISING del encoder
static void IRAM_ATTR encoder_isr() {
    isrPulseCount++;
}

void encoder_init() {
    pinMode(PIN_ENCODER, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER), encoder_isr, RISING);
    lastCalcTime = millis();
}

void encoder_update() {
    unsigned long now = millis();
    unsigned long elapsed = now - lastCalcTime;

    if (elapsed >= ENCODER_CALC_MS) {
        // Leer el contador ISR de forma atómica
        noInterrupts();
        totalPulses = isrPulseCount;
        interrupts();

        uint32_t deltaPulses = totalPulses - lastPulses;
        float elapsedSec = elapsed / 1000.0f;

        // Pulsos por segundo → revoluciones por segundo
        float pulsesPerSec = deltaPulses / elapsedSec;
        rps = pulsesPerSec / PULSES_PER_REV;
        rpm = rps * 60.0f;

        lastPulses   = totalPulses;
        lastCalcTime = now;
    }
}

uint32_t encoder_getPulseCount() {
    noInterrupts();
    uint32_t count = isrPulseCount;
    interrupts();
    return count;
}

float encoder_getTurns() {
    return (float)encoder_getPulseCount() / PULSES_PER_REV;
}

float encoder_getRPS() {
    return rps;
}

float encoder_getRPM() {
    return rpm;
}
