/*
 * ============================================================
 *  TEST: Limit Switches XV-152-1C25
 *  Proyecto Péndulo Invertido — Fase 1
 * ============================================================
 *
 *  Objetivo:
 *    Verificar conexiones y funcionamiento de los 2 limit
 *    switches instalados en los extremos de la pista.
 *
 *  Hardware:
 *    - ESP32-WROOM-32 DevKitC
 *    - XV-152-1C25 x2 (configuración NC + Pull-Up interno)
 *
 *  Conexión:
 *    LS_INICIO: Pin C → GND, Pin NC → GPIO 32
 *    LS_FIN:    Pin C → GND, Pin NC → GPIO 33
 *    (Pin NO sin conectar en ambos)
 *
 *  Lógica (NC + INPUT_PULLUP):
 *    - Switch libre (carro lejos): NC cerrado → GND → LOW
 *    - Switch presionado (carro en límite): NC abre → Pull-up → HIGH
 *    - Cable roto/desconectado: circuito abierto → Pull-up → HIGH (fail-safe)
 *
 *  Salida Serial (115200 baud):
 *    Imprime estado cada 200 ms.
 *    Al detectar activación por ISR, imprime mensaje especial.
 *
 * ============================================================
 */

#include <Arduino.h>

// ============================================================
// Pines
// ============================================================
#define PIN_LIMIT_START  32   // Limit switch INICIO (posición 0)
#define PIN_LIMIT_END    33   // Limit switch FIN (posición ~95 cm)
#define PIN_LED          13   // LED indicador

// ============================================================
// Tiempos
// ============================================================
#define PRINT_INTERVAL_MS  200  // Intervalo de impresión serial

// ============================================================
// Variables ISR (volatile)
// ============================================================
volatile bool limitStartTriggered = false;
volatile bool limitEndTriggered   = false;

volatile unsigned long lastISR_start = 0;
volatile unsigned long lastISR_end   = 0;

// Debounce mínimo en ISR (microsegundos)
#define DEBOUNCE_US 50000  // 50 ms

// ============================================================
// ISR — Limit Switch INICIO
// ============================================================
void IRAM_ATTR isr_limit_start() {
    unsigned long now = micros();
    if ((now - lastISR_start) > DEBOUNCE_US) {
        limitStartTriggered = true;
        lastISR_start = now;
    }
}

// ============================================================
// ISR — Limit Switch FIN
// ============================================================
void IRAM_ATTR isr_limit_end() {
    unsigned long now = micros();
    if ((now - lastISR_end) > DEBOUNCE_US) {
        limitEndTriggered = true;
        lastISR_end = now;
    }
}

// ============================================================
// Contadores de activaciones
// ============================================================
unsigned long startCount = 0;
unsigned long endCount   = 0;

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1500);

    // Configurar pines
    pinMode(PIN_LIMIT_START, INPUT_PULLUP);
    pinMode(PIN_LIMIT_END,   INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Registrar interrupciones (RISING = LOW→HIGH = switch activado)
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_START), isr_limit_start, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_END),   isr_limit_end,   RISING);

    // Header
    Serial.println("=============================================================");
    Serial.println("  TEST: Limit Switches XV-152-1C25");
    Serial.println("  LS_INICIO → GPIO 32 | LS_FIN → GPIO 33");
    Serial.println("  Configuración: NC + INPUT_PULLUP");
    Serial.println("  LOW = libre | HIGH = activado / fail-safe");
    Serial.println("=============================================================");
    Serial.println();
    Serial.printf("%-14s | %-14s | %-6s | %-10s | %-10s\n",
                  "LS_INICIO", "LS_FIN", "LED", "ISR_Start", "ISR_End");
    Serial.println("--------------------------------------------------------------");
}

// ============================================================
// Loop
// ============================================================
void loop() {
    static unsigned long lastPrint = 0;

    // --- Procesar flags ISR ---
    if (limitStartTriggered) {
        limitStartTriggered = false;
        startCount++;
        Serial.printf(">>> ISR: Limit Switch INICIO ACTIVADO! (total: %lu)\n", startCount);
    }

    if (limitEndTriggered) {
        limitEndTriggered = false;
        endCount++;
        Serial.printf(">>> ISR: Limit Switch FIN ACTIVADO! (total: %lu)\n", endCount);
    }

    // --- Lectura directa de estado ---
    bool stateStart = digitalRead(PIN_LIMIT_START);  // HIGH = activado
    bool stateEnd   = digitalRead(PIN_LIMIT_END);    // HIGH = activado

    // LED encendido si cualquier switch está activado
    bool anyActive = stateStart || stateEnd;
    digitalWrite(PIN_LED, anyActive ? HIGH : LOW);

    // --- Imprimir estado periódicamente ---
    unsigned long now = millis();
    if ((now - lastPrint) >= PRINT_INTERVAL_MS) {
        lastPrint = now;

        Serial.printf("%-14s | %-14s | %-6s | %-10lu | %-10lu\n",
                      stateStart ? "ACTIVADO" : "LIBRE",
                      stateEnd   ? "ACTIVADO" : "LIBRE",
                      anyActive  ? "ON" : "OFF",
                      startCount,
                      endCount);
    }
}
