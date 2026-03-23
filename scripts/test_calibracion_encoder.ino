/*
 * ============================================================
 *  CALIBRACIÓN: Encoder HC-020K — Distancia lineal
 *  Proyecto Péndulo Invertido
 * ============================================================
 *
 *  Objetivo:
 *    Obtener las constantes de conversión entre pulsos del
 *    encoder IR y la distancia lineal recorrida por el carro
 *    del prototipo (91 cm de recorrido operativo).
 *
 *  Secuencia:
 *    1. Verifica limit switch INICIO. Si no está activado,
 *       mueve el motor ATRÁS hasta activarlo y se detiene.
 *    2. Espera 3 segundos.
 *    3. Avanza ADELANTE a PWM 100 registrando pulsos del
 *       encoder hasta que se active el limit switch FIN.
 *    4. Detiene el motor e imprime resumen de calibración:
 *       pulsos totales, vueltas, pulsos/cm, cm/pulso, etc.
 *
 *  Hardware:
 *    - ESP32-WROOM-32 DevKitC
 *    - Motor DC + BTS7960 H-Bridge
 *    - Encoder HC-020K (PPR = 20 ranuras → 40 flancos si se
 *      usa RISING+FALLING; aquí solo RISING → 20 por vuelta
 *      mecánica de la rueda, pero el firmware previo usó 40
 *      como PULSES_PER_REV → se mantiene la convención)
 *    - Limit Switch XV-152-1C25 x2 (NC + Pull-Up interno)
 *
 *  Pines:
 *    Motor BTS7960:
 *      R_EN  → GPIO 27 | L_EN  → GPIO 26
 *      R_PWM → GPIO 14 | L_PWM → GPIO 12
 *    Encoder HC-020K:
 *      OUT   → GPIO 16 (interrupt RISING)
 *    Limit Switches:
 *      LS_INICIO → GPIO 32 | LS_FIN → GPIO 33
 *      (NC + INPUT_PULLUP: HIGH = activado)
 *
 *  Salida Serial: 115200 baud
 *
 * ============================================================
 */

#include <Arduino.h>

// ============================================================
// Pines — Motor BTS7960
// ============================================================
#define PIN_R_EN   26
#define PIN_L_EN   27
#define PIN_R_PWM  12
#define PIN_L_PWM  14

// ============================================================
// Pines — Encoder HC-020K
// ============================================================
#define PIN_ENCODER 16

// ============================================================
// Pines — Limit Switches
// ============================================================
#define PIN_LIMIT_START  32   // Limit switch INICIO (posición 0)
#define PIN_LIMIT_END    33   // Limit switch FIN

// ============================================================
// Configuración PWM (LEDC)
// ============================================================
#define PWM_FREQ        25000   // 25 kHz
#define PWM_RESOLUTION  10      // 10-bit (0-1023)
#define LEDC_CH_R       0       // Canal LEDC para R_PWM
#define LEDC_CH_L       1       // Canal LEDC para L_PWM

// ============================================================
// Encoder
// ============================================================
#define PULSES_PER_REV  40      // Convención del proyecto

// ============================================================
// Parámetros de calibración
// ============================================================
#define TRAVEL_DISTANCE_CM  91.0f   // Distancia lineal inicio→fin
#define CALIBRATION_PWM     300     // Velocidad PWM para el recorrido
#define HOMING_PWM          300     // Velocidad PWM para ir a inicio
#define WAIT_BEFORE_RUN_MS  3000    // Espera antes de avanzar (ms)

// ============================================================
// Tiempos
// ============================================================
#define ENCODER_CALC_MS  500    // Ventana de cálculo de velocidad
#define SERIAL_PRINT_MS  250    // Intervalo de impresión en carrera

// ============================================================
// Debounce ISR limit switches (microsegundos)
// ============================================================
#define DEBOUNCE_US 50000  // 50 ms

// ============================================================
// Estados de la máquina
// ============================================================
enum CalState {
    ST_CHECK_HOME,      // Verificar si está en inicio
    ST_HOMING,          // Retrocediendo hacia limit switch inicio
    ST_AT_HOME,         // En inicio, esperando 3 s
    ST_RUNNING,         // Avanzando hacia limit switch fin
    ST_DONE             // Recorrido completo, mostrar resultados
};

// ============================================================
// Variables del encoder (ISR)
// ============================================================
volatile uint32_t isrPulseCount = 0;

static uint32_t runStartPulses  = 0;   // Pulsos al iniciar carrera
static uint32_t lastCalcPulses  = 0;
static unsigned long lastCalcTime = 0;
static float rps = 0.0f;
static float rpm = 0.0f;

// ============================================================
// Variables limit switches (ISR)
// ============================================================
volatile bool limitStartTriggered = false;
volatile bool limitEndTriggered   = false;
volatile unsigned long lastISR_start = 0;
volatile unsigned long lastISR_end   = 0;

// ============================================================
// Variables de estado
// ============================================================
CalState currentState = ST_CHECK_HOME;
unsigned long stateEntryTime = 0;
unsigned long lastPrintTime  = 0;
unsigned long runStartTime   = 0;

// ============================================================
// ISR — Encoder
// ============================================================
void IRAM_ATTR encoder_isr() {
    isrPulseCount++;
}

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
// Funciones del motor (BTS7960)
// ============================================================
void motorAdelante(int pwm) {
    ledcWrite(LEDC_CH_L, 0);
    ledcWrite(LEDC_CH_R, pwm);
}

void motorAtras(int pwm) {
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, pwm);
}

void motorFreno() {
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, 0);
}

// ============================================================
// Lectura atómica de pulsos
// ============================================================
uint32_t getPulseCount() {
    noInterrupts();
    uint32_t count = isrPulseCount;
    interrupts();
    return count;
}

// ============================================================
// Resetear contador de pulsos
// ============================================================
void resetPulseCount() {
    noInterrupts();
    isrPulseCount = 0;
    interrupts();
}

// ============================================================
// Actualizar cálculos de velocidad del encoder
// ============================================================
void encoderUpdate() {
    unsigned long now = millis();
    unsigned long elapsed = now - lastCalcTime;

    if (elapsed >= ENCODER_CALC_MS) {
        uint32_t currentPulses = getPulseCount();
        uint32_t deltaPulses = currentPulses - lastCalcPulses;
        float elapsedSec = elapsed / 1000.0f;

        float pulsesPerSec = deltaPulses / elapsedSec;
        rps = pulsesPerSec / PULSES_PER_REV;
        rpm = rps * 60.0f;

        lastCalcPulses = currentPulses;
        lastCalcTime   = now;
    }
}

// ============================================================
// Cambiar estado
// ============================================================
void changeState(CalState newState) {
    currentState   = newState;
    stateEntryTime = millis();
}

// ============================================================
// Imprimir resultados de calibración
// ============================================================
void printCalibrationResults(uint32_t totalPulses, unsigned long elapsedMs) {
    float totalTurns    = (float)totalPulses / PULSES_PER_REV;
    float elapsedSec    = elapsedMs / 1000.0f;
    float pulsesPerCm   = (float)totalPulses / TRAVEL_DISTANCE_CM;
    float cmPerPulse    = TRAVEL_DISTANCE_CM / (float)totalPulses;
    float turnsForTravel = totalTurns;
    float cmPerTurn     = TRAVEL_DISTANCE_CM / totalTurns;
    float avgPulsesPerSec = (float)totalPulses / elapsedSec;
    float avgRPS        = avgPulsesPerSec / PULSES_PER_REV;
    float avgRPM        = avgRPS * 60.0f;
    float avgSpeedCmS   = TRAVEL_DISTANCE_CM / elapsedSec;

    Serial.println();
    Serial.println("╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║         RESULTADOS DE CALIBRACIÓN — ENCODER HC-020K      ║");
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.println("║  PARÁMETROS DEL RECORRIDO                                ║");
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.printf( "║  Distancia lineal:        %8.2f cm                    ║\n", TRAVEL_DISTANCE_CM);
    Serial.printf( "║  Velocidad PWM:           %8d /1023                  ║\n", CALIBRATION_PWM);
    Serial.printf( "║  Tiempo del recorrido:    %8.3f s                     ║\n", elapsedSec);
    Serial.printf( "║  Velocidad promedio:      %8.2f cm/s                  ║\n", avgSpeedCmS);
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.println("║  MEDICIONES DEL ENCODER                                  ║");
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.printf( "║  Pulsos totales:          %8lu                       ║\n", (unsigned long)totalPulses);
    Serial.printf( "║  Vueltas totales:         %8.2f                       ║\n", totalTurns);
    Serial.printf( "║  Pulsos por vuelta (PPR): %8d  (configurado)        ║\n", PULSES_PER_REV);
    Serial.printf( "║  RPM promedio:            %8.2f                       ║\n", avgRPM);
    Serial.printf( "║  RPS promedio:            %8.2f                       ║\n", avgRPS);
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.println("║  CONSTANTES DE CALIBRACIÓN                               ║");
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.printf( "║  PULSOS_POR_CM:           %8.4f  pulsos/cm           ║\n", pulsesPerCm);
    Serial.printf( "║  CM_POR_PULSO:            %8.4f  cm/pulso            ║\n", cmPerPulse);
    Serial.printf( "║  CM_POR_VUELTA:           %8.4f  cm/vuelta           ║\n", cmPerTurn);
    Serial.printf( "║  VUELTAS_POR_RECORRIDO:   %8.2f  vueltas             ║\n", turnsForTravel);
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.println("║  USO EN FIRMWARE                                         ║");
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    Serial.printf( "║  distancia_cm = pulsos * %.6f;                    ║\n", cmPerPulse);
    Serial.printf( "║  pulsos_necesarios = distancia_cm * %.4f;          ║\n", pulsesPerCm);
    Serial.println("╚═══════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println(">> Calibración completa. Reinicia el ESP32 para repetir.");
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1500);

    // Motor — Enable pins
    pinMode(PIN_R_EN, OUTPUT);
    pinMode(PIN_L_EN, OUTPUT);
    digitalWrite(PIN_R_EN, HIGH);
    digitalWrite(PIN_L_EN, HIGH);

    // Motor — PWM (LEDC)
    ledcSetup(LEDC_CH_R, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(LEDC_CH_L, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_R_PWM, LEDC_CH_R);
    ledcAttachPin(PIN_L_PWM, LEDC_CH_L);
    motorFreno();

    // Encoder — Interrupt
    pinMode(PIN_ENCODER, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER), encoder_isr, RISING);

    // Limit Switches
    pinMode(PIN_LIMIT_START, INPUT_PULLUP);
    pinMode(PIN_LIMIT_END,   INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_START), isr_limit_start, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_END),   isr_limit_end,   RISING);

    // Header
    Serial.println("=============================================================");
    Serial.println("  CALIBRACIÓN: Encoder HC-020K — Distancia lineal");
    Serial.println("  Recorrido operativo: 91 cm (inicio → fin)");
    Serial.println("  Motor PWM: 100/1023 | Encoder PPR: 40");
    Serial.println("=============================================================");
    Serial.println();

    // Inicializar tiempos
    lastCalcTime  = millis();
    lastPrintTime = millis();

    // Comenzar verificación
    changeState(ST_CHECK_HOME);
}

// ============================================================
// Loop
// ============================================================
void loop() {
    unsigned long now = millis();
    bool startActive = digitalRead(PIN_LIMIT_START) == HIGH;
    bool endActive   = digitalRead(PIN_LIMIT_END)   == HIGH;

    // Consumir flags ISR
    if (limitStartTriggered) limitStartTriggered = false;
    if (limitEndTriggered)   limitEndTriggered   = false;

    switch (currentState) {

    // ---------------------------------------------------------
    // 1. Verificar si el carro está en posición de inicio
    // ---------------------------------------------------------
    case ST_CHECK_HOME:
        if (startActive) {
            Serial.println("[OK] Limit switch INICIO activado. Carro en posición.");
            motorFreno();
            changeState(ST_AT_HOME);
        } else {
            Serial.println("[!!] Limit switch INICIO no activado.");
            Serial.println("     Moviendo motor ATRÁS para buscar inicio...");
            motorAtras(HOMING_PWM);
            changeState(ST_HOMING);
        }
        break;

    // ---------------------------------------------------------
    // 2. Retroceder hasta encontrar limit switch de inicio
    // ---------------------------------------------------------
    case ST_HOMING:
        if (startActive) {
            motorFreno();
            Serial.println("[OK] Limit switch INICIO detectado. Motor detenido.");
            changeState(ST_AT_HOME);
        }
        // Imprimir progreso de homing
        if ((now - lastPrintTime) >= 500) {
            lastPrintTime = now;
            Serial.println("     ... buscando inicio (motor ATRÁS)...");
        }
        break;

    // ---------------------------------------------------------
    // 3. En inicio — esperar 3 segundos antes de arrancar
    // ---------------------------------------------------------
    case ST_AT_HOME: {
        unsigned long waited = now - stateEntryTime;
        if (waited >= WAIT_BEFORE_RUN_MS) {
            Serial.println();
            Serial.println(">> Iniciando recorrido de calibración ADELANTE...");
            Serial.println();
            Serial.printf("%-10s | %-7s | %-8s | %-6s | %-8s\n",
                          "Tiempo(s)", "Pulsos", "Vueltas", "RPS", "RPM");
            Serial.println("---------------------------------------------------");

            // Resetear encoder para la carrera de calibración
            resetPulseCount();
            runStartPulses = 0;
            lastCalcPulses = 0;
            lastCalcTime   = now;
            rps = 0;
            rpm = 0;
            runStartTime = now;
            lastPrintTime = now;

            motorAdelante(CALIBRATION_PWM);
            changeState(ST_RUNNING);
        } else {
            // Mostrar cuenta regresiva
            if ((now - lastPrintTime) >= 1000) {
                lastPrintTime = now;
                int remaining = (WAIT_BEFORE_RUN_MS - waited) / 1000 + 1;
                Serial.printf("   Esperando... %d s\n", remaining);
            }
        }
        break;
    }

    // ---------------------------------------------------------
    // 4. Avanzando — medir encoder hasta limit switch fin
    // ---------------------------------------------------------
    case ST_RUNNING: {
        encoderUpdate();

        if (endActive) {
            motorFreno();
            unsigned long runElapsed = now - runStartTime;
            uint32_t totalRunPulses = getPulseCount();

            Serial.println();
            Serial.println(">> Limit switch FIN detectado. Motor detenido.");

            printCalibrationResults(totalRunPulses, runElapsed);
            changeState(ST_DONE);
            break;
        }

        // Imprimir progreso durante la carrera
        if ((now - lastPrintTime) >= SERIAL_PRINT_MS) {
            lastPrintTime = now;

            uint32_t pulses = getPulseCount();
            float turns   = (float)pulses / PULSES_PER_REV;
            float elapsed = (now - runStartTime) / 1000.0f;

            Serial.printf("%10.2f | %7lu | %8.2f | %5.2f | %7.2f\n",
                          elapsed,
                          (unsigned long)pulses,
                          turns,
                          rps,
                          rpm);
        }
        break;
    }

    // ---------------------------------------------------------
    // 5. Calibración completa — no hacer nada más
    // ---------------------------------------------------------
    case ST_DONE:
        // Idle — esperar reinicio
        break;
    }
}
