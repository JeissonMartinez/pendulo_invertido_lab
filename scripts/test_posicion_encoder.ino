/*
 * ============================================================
 *  POSICIÓN LINEAL: Encoder HC-020K + Limit Switches
 *  Proyecto Péndulo Invertido
 * ============================================================
 *
 *  Objetivo:
 *    Medir la posición del carro móvil en centímetros dentro
 *    del rango operativo de 91 cm, usando las constantes
 *    obtenidas en la calibración del encoder IR.
 *
 *  Estrategia de dirección:
 *    El encoder HC-020K es unidireccional (no cuadratura),
 *    por lo que NO detecta dirección de giro. La dirección
 *    se infiere del comando activo del motor:
 *      - Motor ADELANTE → pulsos se SUMAN (posición crece)
 *      - Motor ATRÁS    → pulsos se RESTAN (posición decrece)
 *      - Motor DETENIDO → pulsos se ignoran (ruido/inercia)
 *    Los limit switches actúan como referencia absoluta:
 *      - LS_INICIO activado → posición = 0.00 cm
 *      - LS_FIN activado    → posición = 91.00 cm
 *
 *  Secuencia del test:
 *    1. Homing: llevar carro a posición inicio (0 cm).
 *    2. Ida: ADELANTE a PWM variable, mostrando posición.
 *    3. Al llegar a FIN, pausa y retorno.
 *    4. Vuelta: ATRÁS mostrando posición decreciente.
 *    5. Al llegar a INICIO, muestra resumen de precisión.
 *    6. Repite ciclos indefinidamente.
 *
 *  Hardware:
 *    - ESP32-WROOM-32 DevKitC
 *    - Motor DC + BTS7960 H-Bridge
 *    - Encoder HC-020K
 *    - Limit Switch XV-152-1C25 x2 (NC + Pull-Up)
 *
 *  Pines:
 *    Motor:   R_EN→GPIO 26, L_EN→GPIO 27, R_PWM→GPIO 12, L_PWM→GPIO 14
 *    Encoder: OUT→GPIO 16
 *    LS:      INICIO→GPIO 32, FIN→GPIO 33
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
#define PIN_LIMIT_START  32
#define PIN_LIMIT_END    33

// ============================================================
// Configuración PWM (LEDC)
// ============================================================
#define PWM_FREQ        25000
#define PWM_RESOLUTION  10      // 0-1023
#define LEDC_CH_R       0
#define LEDC_CH_L       1

// ============================================================
// CONSTANTES DE CALIBRACIÓN (obtenidas del script de calibración)
// ============================================================
#define CM_POR_PULSO        0.0504f     // cm que avanza por cada pulso
#define PULSOS_POR_CM       19.8571f    // pulsos por cada cm lineal
#define DISTANCIA_TOTAL_CM  91.0f       // recorrido operativo total
#define PULSES_PER_REV      40          // convención del proyecto

// ============================================================
// Parámetros del test
// ============================================================
#define TEST_PWM            300     // PWM para ida y vuelta
#define HOMING_PWM          300     // PWM para homing
#define PAUSE_AT_END_MS     2000    // Pausa al llegar a cada extremo
#define SERIAL_PRINT_MS     200     // Intervalo de impresión

// ============================================================
// Debounce ISR (microsegundos)
// ============================================================
#define DEBOUNCE_US 50000

// ============================================================
// Dirección del motor (usada para signo del encoder)
// ============================================================
enum MotorDir {
    DIR_STOP     = 0,
    DIR_ADELANTE = 1,   // Posición crece (hacia LS_FIN)
    DIR_ATRAS    = -1   // Posición decrece (hacia LS_INICIO)
};

// ============================================================
// Estados de la máquina
// ============================================================
enum TestState {
    ST_HOMING,          // Buscando limit switch inicio
    ST_WAIT_START,      // Pausa en inicio antes de arrancar
    ST_GOING_FWD,       // Yendo hacia FIN
    ST_WAIT_END,        // Pausa en FIN
    ST_GOING_BACK,      // Regresando a INICIO
    ST_WAIT_HOME,       // Pausa en INICIO tras vuelta
    ST_SHOW_RESULTS     // Muestra resumen de precisión del ciclo
};

// ============================================================
// Variables del encoder — posición con signo
// ============================================================
volatile uint32_t isrPulseCount = 0;
static uint32_t lastReadPulses  = 0;

// Posición en pulsos con signo (puede ser negativa si hay error)
static int32_t positionPulses = 0;

// Dirección actual del motor
static volatile MotorDir motorDirection = DIR_STOP;

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
TestState currentState    = ST_HOMING;
unsigned long stateEntry  = 0;
unsigned long lastPrint   = 0;

// Variables para resumen de ciclo
int32_t pulsesAtFwd       = 0;   // Pulsos acumulados al llegar a FIN
int32_t pulsesAtBack      = 0;   // Pulsos acumulados al volver a INICIO
uint32_t cycleCount       = 0;

// ============================================================
// ISR — Encoder
// ============================================================
void IRAM_ATTR encoder_isr() {
    isrPulseCount++;
}

// ============================================================
// ISR — Limit Switches
// ============================================================
void IRAM_ATTR isr_limit_start() {
    unsigned long now = micros();
    if ((now - lastISR_start) > DEBOUNCE_US) {
        limitStartTriggered = true;
        lastISR_start = now;
    }
}

void IRAM_ATTR isr_limit_end() {
    unsigned long now = micros();
    if ((now - lastISR_end) > DEBOUNCE_US) {
        limitEndTriggered = true;
        lastISR_end = now;
    }
}

// ============================================================
// Funciones del motor
// ============================================================
void motorAdelante(int pwm) {
    motorDirection = DIR_ADELANTE;
    ledcWrite(LEDC_CH_L, 0);
    ledcWrite(LEDC_CH_R, pwm);
}

void motorAtras(int pwm) {
    motorDirection = DIR_ATRAS;
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, pwm);
}

void motorFreno() {
    motorDirection = DIR_STOP;
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, 0);
}

// ============================================================
// Actualizar posición con signo a partir de pulsos ISR
//
// Lee los pulsos brutos del ISR, calcula el delta desde la
// última lectura, y suma o resta según la dirección del motor.
// Cuando el motor está detenido los pulsos se ignoran (ruido
// mecánico o inercia residual no deben alterar la posición).
// ============================================================
void updatePosition() {
    noInterrupts();
    uint32_t raw = isrPulseCount;
    interrupts();

    uint32_t delta = raw - lastReadPulses;
    lastReadPulses = raw;

    if (delta == 0) return;

    switch (motorDirection) {
        case DIR_ADELANTE:
            positionPulses += (int32_t)delta;
            break;
        case DIR_ATRAS:
            positionPulses -= (int32_t)delta;
            break;
        case DIR_STOP:
            // Ignorar pulsos mientras el motor está detenido
            break;
    }
}

// ============================================================
// Obtener posición en centímetros
// ============================================================
float getPositionCm() {
    float cm = (float)positionPulses * CM_POR_PULSO;
    // Clamp al rango operativo
    if (cm < 0.0f)              cm = 0.0f;
    if (cm > DISTANCIA_TOTAL_CM) cm = DISTANCIA_TOTAL_CM;
    return cm;
}

// ============================================================
// Resetear posición (recalibrar en limit switch)
// ============================================================
void setPositionHome() {
    noInterrupts();
    isrPulseCount = 0;
    interrupts();
    lastReadPulses  = 0;
    positionPulses  = 0;
}

void setPositionEnd() {
    int32_t expectedPulses = (int32_t)(DISTANCIA_TOTAL_CM * PULSOS_POR_CM);
    positionPulses = expectedPulses;
}

// ============================================================
// Cambiar estado
// ============================================================
void changeState(TestState s) {
    currentState = s;
    stateEntry   = millis();
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1500);

    // Motor — Enable
    pinMode(PIN_R_EN, OUTPUT);
    pinMode(PIN_L_EN, OUTPUT);
    digitalWrite(PIN_R_EN, HIGH);
    digitalWrite(PIN_L_EN, HIGH);

    // Motor — PWM
    ledcSetup(LEDC_CH_R, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(LEDC_CH_L, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_R_PWM, LEDC_CH_R);
    ledcAttachPin(PIN_L_PWM, LEDC_CH_L);
    motorFreno();

    // Encoder
    pinMode(PIN_ENCODER, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER), encoder_isr, RISING);

    // Limit Switches
    pinMode(PIN_LIMIT_START, INPUT_PULLUP);
    pinMode(PIN_LIMIT_END,   INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_START), isr_limit_start, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_END),   isr_limit_end,   RISING);

    // Header
    Serial.println("=============================================================");
    Serial.println("  POSICIÓN LINEAL: Encoder HC-020K");
    Serial.println("  Constantes: CM_POR_PULSO=0.0504 | PULSOS_POR_CM=19.8571");
    Serial.println("  Rango operativo: 0.00 — 91.00 cm");
    Serial.println("=============================================================");
    Serial.println();

    lastPrint = millis();

    // Verificar si ya está en inicio
    if (digitalRead(PIN_LIMIT_START) == HIGH) {
        Serial.println("[OK] Carro en posición de inicio.");
        setPositionHome();
        changeState(ST_WAIT_START);
    } else {
        Serial.println("[!!] Buscando posición de inicio...");
        motorAtras(HOMING_PWM);
        changeState(ST_HOMING);
    }
}

// ============================================================
// Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // Leer limit switches
    bool startActive = digitalRead(PIN_LIMIT_START) == HIGH;
    bool endActive   = digitalRead(PIN_LIMIT_END)   == HIGH;

    // Consumir flags ISR
    if (limitStartTriggered) limitStartTriggered = false;
    if (limitEndTriggered)   limitEndTriggered   = false;

    // Actualizar posición con signo
    updatePosition();

    // Recalibración automática por limit switches
    // (corrige drift acumulado del encoder)
    if (startActive && motorDirection == DIR_ATRAS) {
        if (positionPulses != 0) {
            Serial.printf("  [RECAL] LS_INICIO: posición corregida de %.2f cm a 0.00 cm\n",
                          (float)positionPulses * CM_POR_PULSO);
        }
        positionPulses = 0;
    }
    if (endActive && motorDirection == DIR_ADELANTE) {
        int32_t expected = (int32_t)(DISTANCIA_TOTAL_CM * PULSOS_POR_CM);
        if (abs(positionPulses - expected) > 2) {
            Serial.printf("  [RECAL] LS_FIN: posición corregida de %.2f cm a %.2f cm\n",
                          (float)positionPulses * CM_POR_PULSO, DISTANCIA_TOTAL_CM);
        }
        positionPulses = expected;
    }

    switch (currentState) {

    // ---------------------------------------------------------
    case ST_HOMING:
        if (startActive) {
            motorFreno();
            setPositionHome();
            Serial.println("[OK] Posición de inicio establecida: 0.00 cm");
            changeState(ST_WAIT_START);
        }
        if ((now - lastPrint) >= 500) {
            lastPrint = now;
            Serial.println("     ... buscando inicio ...");
        }
        break;

    // ---------------------------------------------------------
    case ST_WAIT_START: {
        unsigned long waited = now - stateEntry;
        if (waited >= PAUSE_AT_END_MS) {
            cycleCount++;
            Serial.println();
            Serial.printf("========== CICLO %lu: IDA (→ FIN) ==========\n", (unsigned long)cycleCount);
            Serial.printf("%-10s | %-10s | %-8s | %-12s\n",
                          "Tiempo(s)", "Posición", "Pulsos", "Dirección");
            Serial.println("----------------------------------------------");

            // Guardar pulsos antes de arrancar
            pulsesAtFwd = 0;

            motorAdelante(TEST_PWM);
            changeState(ST_GOING_FWD);
        } else if ((now - lastPrint) >= 1000) {
            lastPrint = now;
            int rem = (PAUSE_AT_END_MS - waited) / 1000 + 1;
            Serial.printf("   Inicio en %d s...\n", rem);
        }
        break;
    }

    // ---------------------------------------------------------
    case ST_GOING_FWD:
        if (endActive) {
            motorFreno();
            pulsesAtFwd = positionPulses;
            float posCm = getPositionCm();
            Serial.println();
            Serial.printf(">> LLEGÓ A FIN | Posición: %.2f cm | Pulsos: %ld\n",
                          posCm, (long)positionPulses);
            changeState(ST_WAIT_END);
            break;
        }
        if ((now - lastPrint) >= SERIAL_PRINT_MS) {
            lastPrint = now;
            float posCm  = getPositionCm();
            float elap   = (now - stateEntry) / 1000.0f;
            Serial.printf("%10.2f | %7.2f cm | %8ld | ADELANTE\n",
                          elap, posCm, (long)positionPulses);
        }
        break;

    // ---------------------------------------------------------
    case ST_WAIT_END: {
        unsigned long waited = now - stateEntry;
        if (waited >= PAUSE_AT_END_MS) {
            Serial.println();
            Serial.printf("========== CICLO %lu: VUELTA (→ INICIO) ==========\n",
                          (unsigned long)cycleCount);
            Serial.printf("%-10s | %-10s | %-8s | %-12s\n",
                          "Tiempo(s)", "Posición", "Pulsos", "Dirección");
            Serial.println("----------------------------------------------");

            motorAtras(TEST_PWM);
            changeState(ST_GOING_BACK);
        } else if ((now - lastPrint) >= 1000) {
            lastPrint = now;
            int rem = (PAUSE_AT_END_MS - waited) / 1000 + 1;
            Serial.printf("   Regreso en %d s...\n", rem);
        }
        break;
    }

    // ---------------------------------------------------------
    case ST_GOING_BACK:
        if (startActive) {
            motorFreno();
            pulsesAtBack = positionPulses;
            float posCm = getPositionCm();
            Serial.println();
            Serial.printf(">> LLEGÓ A INICIO | Posición: %.2f cm | Pulsos: %ld\n",
                          posCm, (long)positionPulses);
            changeState(ST_SHOW_RESULTS);
            break;
        }
        if ((now - lastPrint) >= SERIAL_PRINT_MS) {
            lastPrint = now;
            float posCm  = getPositionCm();
            float elap   = (now - stateEntry) / 1000.0f;
            Serial.printf("%10.2f | %7.2f cm | %8ld | ATRÁS\n",
                          elap, posCm, (long)positionPulses);
        }
        break;

    // ---------------------------------------------------------
    case ST_SHOW_RESULTS: {
        float cmAtFwd  = (float)pulsesAtFwd  * CM_POR_PULSO;
        float cmAtBack = (float)pulsesAtBack * CM_POR_PULSO;
        float errorFwd = cmAtFwd - DISTANCIA_TOTAL_CM;
        float errorBack = cmAtBack;  // Debería ser 0

        Serial.println();
        Serial.println("╔════════════════════════════════════════════════╗");
        Serial.printf( "║  RESUMEN CICLO %lu                              ║\n", (unsigned long)cycleCount);
        Serial.println("╠════════════════════════════════════════════════╣");
        Serial.printf( "║  Pos. medida en FIN:    %7.2f cm              ║\n", cmAtFwd);
        Serial.printf( "║  Pos. esperada en FIN:  %7.2f cm              ║\n", DISTANCIA_TOTAL_CM);
        Serial.printf( "║  Error en FIN:          %+7.2f cm              ║\n", errorFwd);
        Serial.println("║                                                ║");
        Serial.printf( "║  Pos. medida en INICIO: %7.2f cm              ║\n", cmAtBack);
        Serial.printf( "║  Pos. esperada INICIO:  %7.2f cm              ║\n", 0.0f);
        Serial.printf( "║  Error en INICIO:       %+7.2f cm              ║\n", errorBack);
        Serial.println("║                                                ║");
        Serial.printf( "║  Pulsos ida:            %7ld                  ║\n", (long)pulsesAtFwd);
        Serial.printf( "║  Pulsos vuelta (neto):  %7ld                  ║\n", (long)pulsesAtBack);
        Serial.printf( "║  Drift acumulado:       %7ld pulsos           ║\n", (long)pulsesAtBack);
        Serial.println("╚════════════════════════════════════════════════╝");
        Serial.println();

        // Resetear posición en home para el siguiente ciclo
        setPositionHome();
        changeState(ST_WAIT_START);
        break;
    }
    }
}
