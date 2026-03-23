/*
 * ============================================================
 *  TEST: Encoder Infrarrojo HC-020K + Motor BTS7960
 *  Proyecto Péndulo Invertido — Fase 2
 * ============================================================
 *
 *  Objetivo:
 *    Verificar funcionamiento del encoder infrarrojo HC-020K
 *    conectado al motor del prototipo real, usando el driver
 *    BTS7960 y la configuración de pines del firmware SPISC.
 *
 *  Hardware:
 *    - ESP32-WROOM-32 DevKitC
 *    - Motor DC + BTS7960 H-Bridge
 *    - Encoder HC-020K (PPR = 20)
 *
 *  Pines:
 *    Motor BTS7960:
 *      R_EN  → GPIO 26 (enable derecho)
 *      L_EN  → GPIO 27 (enable izquierdo)
 *      R_PWM → GPIO 14 (PWM canal 0, 25 kHz 10-bit)
 *      L_PWM → GPIO 12 (PWM canal 1, 25 kHz 10-bit)
 *    Encoder HC-020K:
 *      OUT   → GPIO 16 (interrupt RISING)
 *      VCC   → 5V
 *      GND   → GND
 *
 *  Secuencia de estados (4 s cada uno):
 *    STOP_INIT → FWD_SLOW → FWD_MED → FWD_FAST →
 *    STOP_MID → REV_SLOW → REV_MED → REV_FAST → STOP_END
 *
 *  Salida Serial (115200 baud):
 *    Imprime tabla de estado cada 500 ms.
 *
 * ============================================================
 */

#include <Arduino.h>

// ============================================================
// Pines — Motor BTS7960
// ============================================================
#define PIN_R_EN   27
#define PIN_L_EN   26
#define PIN_R_PWM  14
#define PIN_L_PWM  12

// ============================================================
// Pines — Encoder HC-020K
// ============================================================
#define PIN_ENCODER 16

// ============================================================
// Configuración PWM (LEDC) — misma que SPISC
// ============================================================
#define PWM_FREQ        25000   // 25 kHz
#define PWM_RESOLUTION  10      // 10-bit (0-1023)
#define LEDC_CH_R       0       // Canal LEDC para R_PWM
#define LEDC_CH_L       1       // Canal LEDC para L_PWM

// ============================================================
// Encoder
// ============================================================
#define PULSES_PER_REV  40      // 20 ranuras en la rueda HC-020K

// ============================================================
// Tiempos
// ============================================================
#define STATE_DURATION_MS   500    // Duración de cada estado
#define ENCODER_CALC_MS     1000    // Ventana de cálculo de velocidad
#define SERIAL_PRINT_MS     500     // Intervalo de impresión

// ============================================================
// Niveles de velocidad PWM (10-bit: 0-1023)
// ============================================================
#define SPEED_SLOW  100
#define SPEED_MED   200
#define SPEED_FAST  300

// ============================================================
// Estados
// ============================================================
enum State {
    STOP_INIT,
    FWD_SLOW,
    REV_SLOW,
    FWD_MED,
    REV_MED,
    FWD_FAST,
    REV_FAST,
    STOP_MID,
    NUM_STATES
};

const char* stateNames[] = {
    "STOP_INIT", "FWD_SLOW", "FWD_MED", "FWD_FAST",
    "STOP_MID",  "REV_SLOW", "REV_MED", "REV_FAST",
    "STOP_END"
};

// PWM por estado
const int statePWM[] = {
    0,          SPEED_SLOW, SPEED_MED, SPEED_FAST,
    0,          SPEED_SLOW, SPEED_MED, SPEED_FAST,
    0
};

// Dirección: 0=stop, 1=forward, 2=reverse
const int stateDir[] = {
    0, 1, 1, 1,
    0, 2, 2, 2,
    0
};

// ============================================================
// Variables del motor
// ============================================================
int currentPWM = 0;
const char* currentDirStr = "DETENIDO";

// ============================================================
// Variables del encoder (ISR)
// ============================================================
volatile uint32_t isrPulseCount = 0;

static uint32_t totalPulses   = 0;
static uint32_t lastPulses    = 0;
static unsigned long lastCalcTime = 0;
static float rps = 0.0f;
static float rpm = 0.0f;

// ============================================================
// Variables de estado
// ============================================================
State currentState = STOP_INIT;
unsigned long stateStartTime = 0;
unsigned long lastPrintTime  = 0;

// ============================================================
// ISR — Encoder
// ============================================================
void IRAM_ATTR encoder_isr() {
    isrPulseCount++;
}

// ============================================================
// Funciones del motor (BTS7960)
// ============================================================
void motorAdelante(int pwm) {
    currentPWM = pwm;
    currentDirStr = "ADELANTE";
    ledcWrite(LEDC_CH_L, 0);
    ledcWrite(LEDC_CH_R, pwm);
}

void motorAtras(int pwm) {
    currentPWM = pwm;
    currentDirStr = "ATRAS";
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, pwm);
}

void motorFreno() {
    currentPWM = 0;
    currentDirStr = "DETENIDO";
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, 0);
}

// ============================================================
// Aplicar estado al motor
// ============================================================
void applyState(State s) {
    int pwm = statePWM[s];
    int dir = stateDir[s];

    switch (dir) {
        case 1:  motorAdelante(pwm); break;
        case 2:  motorAtras(pwm);    break;
        default: motorFreno();        break;
    }
}

// ============================================================
// Actualizar encoder (calcular velocidad cada ENCODER_CALC_MS)
// ============================================================
void encoderUpdate() {
    unsigned long now = millis();
    unsigned long elapsed = now - lastCalcTime;

    if (elapsed >= ENCODER_CALC_MS) {
        noInterrupts();
        totalPulses = isrPulseCount;
        interrupts();

        uint32_t deltaPulses = totalPulses - lastPulses;
        float elapsedSec = elapsed / 1000.0f;

        float pulsesPerSec = deltaPulses / elapsedSec;
        rps = pulsesPerSec / PULSES_PER_REV;
        rpm = rps * 60.0f;

        lastPulses   = totalPulses;
        lastCalcTime = now;
    }
}

// ============================================================
// Obtener pulsos de forma atómica
// ============================================================
uint32_t getPulseCount() {
    noInterrupts();
    uint32_t count = isrPulseCount;
    interrupts();
    return count;
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

    // Encoder — Interrupt
    pinMode(PIN_ENCODER, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER), encoder_isr, RISING);

    // Header
    Serial.println("=============================================================");
    Serial.println("  TEST: Encoder HC-020K + Motor BTS7960");
    Serial.println("  ESP32-WROOM-32 | PPR: 20 | PWM: 25kHz 10-bit");
    Serial.println("  Encoder → GPIO 16 | Motor → GPIO 14,12,26,27");
    Serial.println("=============================================================");
    Serial.println();
    Serial.printf("%-12s | %-4s | %-10s | %-7s | %-8s | %-6s | %-8s\n",
                  "Estado", "PWM", "Direccion", "Pulsos", "Vueltas", "RPS", "RPM");
    Serial.println("--------------------------------------------------------------");

    // Estado inicial
    stateStartTime = millis();
    lastCalcTime   = millis();
    lastPrintTime  = millis();
    applyState(STOP_INIT);
}

// ============================================================
// Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // --- Transición de estado ---
    if ((now - stateStartTime) >= STATE_DURATION_MS) {
        int next = (int)currentState + 1;
        if (next >= NUM_STATES) {
            next = 0;  // Reiniciar ciclo
        }
        currentState = (State)next;
        stateStartTime = now;
        applyState(currentState);
    }

    // --- Actualizar encoder ---
    encoderUpdate();

    // --- Imprimir estado ---
    if ((now - lastPrintTime) >= SERIAL_PRINT_MS) {
        lastPrintTime = now;

        uint32_t pulses = getPulseCount();
        float turns = (float)pulses / PULSES_PER_REV;

        Serial.printf("%-12s | %4d | %-10s | %7lu | %8.2f | %5.2f | %7.2f\n",
                      stateNames[currentState],
                      currentPWM,
                      currentDirStr,
                      (unsigned long)pulses,
                      turns,
                      rps,
                      rpm);
    }
}
