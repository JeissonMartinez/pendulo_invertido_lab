/*
 * ============================================================
 *  VALIDACIÓN: Posición por comando serial
 *  Proyecto Péndulo Invertido
 * ============================================================
 *
 *  Objetivo:
 *    Mover el carro a una posición deseada (en cm) recibida
 *    por puerto serial, para validar con medición física la
 *    precisión de las constantes de calibración del encoder.
 *
 *  Uso:
 *    1. El carro hace homing automático al inicio.
 *    2. Enviar por serial un número decimal (ej: "45.5")
 *       y el carro se moverá a esa posición en cm.
 *    3. Al llegar, muestra posición final y error estimado.
 *    4. Se puede enviar otra posición inmediatamente.
 *    5. Enviar "home" para volver a posición 0.
 *    6. Enviar "pos" para consultar la posición actual.
 *
 *  Rango válido: 0.00 — 91.00 cm
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
// CONSTANTES DE CALIBRACIÓN
// ============================================================
#define CM_POR_PULSO        0.0504f
#define PULSOS_POR_CM       19.8413f    // 1806 / 91.0
#define DISTANCIA_TOTAL_CM  91.0f
#define PULSES_PER_REV      40

// ============================================================
// Parámetros de movimiento
// ============================================================
#define MOVE_PWM            200     // PWM para movimiento a posición
#define HOMING_PWM          300     // PWM para homing
#define TOLERANCE_PULSES    3       // Tolerancia de llegada (~0.15 cm)
#define SERIAL_PRINT_MS     300     // Intervalo de impresión en movimiento

// ============================================================
// Debounce ISR (microsegundos)
// ============================================================
#define DEBOUNCE_US 50000

// ============================================================
// Dirección del motor
// ============================================================
enum MotorDir {
    DIR_STOP     = 0,
    DIR_ADELANTE = 1,
    DIR_ATRAS    = -1
};

// ============================================================
// Estados
// ============================================================
enum AppState {
    ST_HOMING,
    ST_IDLE,            // Esperando comando serial
    ST_MOVING,          // Moviendo hacia posición objetivo
    ST_ARRIVED          // Llegó — muestra resultado y pasa a IDLE
};

// ============================================================
// Variables del encoder — posición con signo
// ============================================================
volatile uint32_t isrPulseCount = 0;
static uint32_t lastReadPulses  = 0;
static int32_t positionPulses   = 0;
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
AppState currentState     = ST_HOMING;
unsigned long lastPrint   = 0;

// Posición objetivo
int32_t targetPulses      = 0;
float   targetCm          = 0.0f;

// Buffer de entrada serial
#define SERIAL_BUF_SIZE 32
char serialBuf[SERIAL_BUF_SIZE];
int  serialIdx = 0;

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
// Actualizar posición con signo
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
            break;
    }
}

// ============================================================
// Obtener posición en centímetros (sin clamp, para ver errores)
// ============================================================
float getPositionCm() {
    return (float)positionPulses * CM_POR_PULSO;
}

// ============================================================
// Resetear posición en home
// ============================================================
void setPositionHome() {
    noInterrupts();
    isrPulseCount = 0;
    interrupts();
    lastReadPulses  = 0;
    positionPulses  = 0;
}

// ============================================================
// Cambiar estado
// ============================================================
void changeState(AppState s) {
    currentState = s;
}

// ============================================================
// Mostrar prompt
// ============================================================
void showPrompt() {
    float cm = getPositionCm();
    Serial.println();
    Serial.println("------------------------------------------------------------");
    Serial.printf("  Posición actual: %.2f cm  (%ld pulsos)\n", cm, (long)positionPulses);
    Serial.println("  Enviar posición deseada en cm (0.00 — 91.00)");
    Serial.println("  Comandos: 'home' = ir a 0 | 'pos' = ver posición actual");
    Serial.println("------------------------------------------------------------");
    Serial.print(">> ");
}

// ============================================================
// Procesar comando serial
// ============================================================
void processCommand(const char* cmd) {
    // Comando "home"
    if (strncmp(cmd, "home", 4) == 0) {
        targetCm = 0.0f;
        targetPulses = 0;
        Serial.printf("\n[CMD] Ir a HOME (0.00 cm)\n");

        if (abs(positionPulses - targetPulses) <= TOLERANCE_PULSES) {
            Serial.println("[OK] Ya está en home.");
            showPrompt();
            return;
        }
        motorAtras(MOVE_PWM);
        changeState(ST_MOVING);
        return;
    }

    // Comando "pos"
    if (strncmp(cmd, "pos", 3) == 0) {
        float cm = getPositionCm();
        Serial.printf("\n[POS] Posición actual: %.2f cm  (%ld pulsos)\n",
                      cm, (long)positionPulses);
        Serial.print(">> ");
        return;
    }

    // Intentar parsear como número decimal
    char* endPtr;
    float requested = strtof(cmd, &endPtr);

    // Verificar que se parseó algo válido
    if (endPtr == cmd) {
        Serial.printf("\n[ERROR] Comando no reconocido: '%s'\n", cmd);
        Serial.print(">> ");
        return;
    }

    // Validar rango
    if (requested < 0.0f || requested > DISTANCIA_TOTAL_CM) {
        Serial.printf("\n[ERROR] Fuera de rango. Usar 0.00 — %.2f cm\n", DISTANCIA_TOTAL_CM);
        Serial.print(">> ");
        return;
    }

    targetCm     = requested;
    targetPulses = (int32_t)(requested * PULSOS_POR_CM);

    int32_t diff = targetPulses - positionPulses;
    float currentCm = getPositionCm();

    Serial.printf("\n[CMD] Ir a %.2f cm  (actual: %.2f cm | delta: %+.2f cm)\n",
                  targetCm, currentCm, targetCm - currentCm);

    // Ya estamos dentro de tolerancia
    if (abs(diff) <= TOLERANCE_PULSES) {
        Serial.println("[OK] Ya está en la posición solicitada.");
        showPrompt();
        return;
    }

    // Mover en la dirección correcta
    if (diff > 0) {
        Serial.println("     Moviendo ADELANTE...");
        motorAdelante(MOVE_PWM);
    } else {
        Serial.println("     Moviendo ATRÁS...");
        motorAtras(MOVE_PWM);
    }
    changeState(ST_MOVING);
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
    Serial.println("  VALIDACIÓN DE POSICIÓN — Encoder HC-020K");
    Serial.println("  CM_POR_PULSO=0.0504 | PULSOS_POR_CM=19.8413");
    Serial.println("  Rango: 0.00 — 91.00 cm | PWM movimiento: 200");
    Serial.println("=============================================================");
    Serial.println();

    lastPrint = millis();

    // Homing automático
    if (digitalRead(PIN_LIMIT_START) == HIGH) {
        Serial.println("[OK] Carro ya en posición de inicio.");
        setPositionHome();
        changeState(ST_IDLE);
        showPrompt();
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

    // Actualizar posición
    updatePosition();

    // --- Seguridad: detener si limit switch se activa en movimiento ---
    if (startActive && motorDirection == DIR_ATRAS && currentState == ST_MOVING) {
        motorFreno();
        positionPulses = 0;  // Recalibrar a home
        Serial.println("\n  [SEGURIDAD] Limit switch INICIO alcanzado. Motor detenido.");
        Serial.printf("  Posición recalibrada a 0.00 cm\n");
        changeState(ST_ARRIVED);
    }
    if (endActive && motorDirection == DIR_ADELANTE && currentState == ST_MOVING) {
        motorFreno();
        int32_t expected = (int32_t)(DISTANCIA_TOTAL_CM * PULSOS_POR_CM);
        positionPulses = expected;  // Recalibrar a fin
        Serial.println("\n  [SEGURIDAD] Limit switch FIN alcanzado. Motor detenido.");
        Serial.printf("  Posición recalibrada a %.2f cm\n", DISTANCIA_TOTAL_CM);
        changeState(ST_ARRIVED);
    }

    switch (currentState) {

    // ---------------------------------------------------------
    case ST_HOMING:
        if (startActive) {
            motorFreno();
            setPositionHome();
            Serial.println("[OK] Posición de inicio establecida: 0.00 cm");
            changeState(ST_IDLE);
            showPrompt();
        }
        if ((now - lastPrint) >= 500) {
            lastPrint = now;
            Serial.println("     ... buscando inicio ...");
        }
        break;

    // ---------------------------------------------------------
    case ST_IDLE: {
        // Leer entrada serial carácter por carácter
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (serialIdx > 0) {
                    serialBuf[serialIdx] = '\0';
                    serialIdx = 0;
                    processCommand(serialBuf);
                }
            } else if (serialIdx < SERIAL_BUF_SIZE - 1) {
                serialBuf[serialIdx++] = c;
            }
        }
        break;
    }

    // ---------------------------------------------------------
    case ST_MOVING: {
        int32_t diff = targetPulses - positionPulses;

        // Llegamos dentro de tolerancia
        if (abs(diff) <= TOLERANCE_PULSES) {
            motorFreno();
            changeState(ST_ARRIVED);
            break;
        }

        // Verificar si nos pasamos (cambio de signo del error)
        // Si íbamos ADELANTE pero ahora diff < 0 (nos pasamos)
        if (motorDirection == DIR_ADELANTE && diff < -TOLERANCE_PULSES) {
            motorFreno();
            changeState(ST_ARRIVED);
            break;
        }
        // Si íbamos ATRÁS pero ahora diff > 0 (nos pasamos)
        if (motorDirection == DIR_ATRAS && diff > TOLERANCE_PULSES) {
            motorFreno();
            changeState(ST_ARRIVED);
            break;
        }

        // Imprimir progreso
        if ((now - lastPrint) >= SERIAL_PRINT_MS) {
            lastPrint = now;
            float cm = getPositionCm();
            float remaining = targetCm - cm;
            Serial.printf("  pos: %7.2f cm | objetivo: %7.2f cm | faltan: %+7.2f cm | pulsos: %ld\n",
                          cm, targetCm, remaining, (long)positionPulses);
        }
        break;
    }

    // ---------------------------------------------------------
    case ST_ARRIVED: {
        float finalCm = getPositionCm();
        float errorCm = finalCm - targetCm;

        Serial.println();
        Serial.println("╔══════════════════════════════════════════╗");
        Serial.println("║          POSICIÓN ALCANZADA              ║");
        Serial.println("╠══════════════════════════════════════════╣");
        Serial.printf( "║  Objetivo:     %7.2f cm                 ║\n", targetCm);
        Serial.printf( "║  Medida:       %7.2f cm                 ║\n", finalCm);
        Serial.printf( "║  Error:        %+7.2f cm                 ║\n", errorCm);
        Serial.printf( "║  Pulsos:       %7ld                     ║\n", (long)positionPulses);
        Serial.println("╚══════════════════════════════════════════╝");

        changeState(ST_IDLE);
        showPrompt();
        break;
    }
    }
}
