/*
 * ============================================================
 *  RANGO OPERATIVO PWM: Barrido de velocidad del motor
 *  Proyecto Péndulo Invertido
 * ============================================================
 *
 *  Objetivo:
 *    Encontrar el PWM mínimo (que logra mover el carro) y el
 *    PWM máximo útil (antes de que la inercia sea excesiva),
 *    mediante un barrido incremental de PWM de 1 en 1.
 *
 *  Secuencia por cada valor de PWM:
 *    1. Desde HOME, mover ADELANTE con el PWM actual.
 *    2. Si en 5 s no llega a LS_FIN → preguntar al usuario
 *       si el carro se movió (SI/NO).
 *       - NO  → registrar como "no se mueve", siguiente PWM.
 *       - SI  → esperar a que llegue a LS_FIN.
 *    3. Si llega a LS_FIN → esperar 2 s, regresar ATRÁS
 *       con el mismo PWM hasta LS_INICIO.
 *    4. Registrar datos: pulsos, tiempo, RPM.
 *    5. Siguiente PWM (+1).
 *
 *  Comandos serial (en cualquier momento):
 *    "stop"      → Detiene el barrido. Registra PWM actual
 *                   como máximo. Muestra tabla de resultados.
 *    "reiniciar" → (después de resultados) Repite todo.
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
 *  Serial: 115200 baud
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
// Encoder
// ============================================================
#define PULSES_PER_REV  40
#define CM_POR_PULSO    0.0504f

// ============================================================
// Parámetros del barrido
// ============================================================
#define PWM_START           1       // PWM inicial del barrido
#define PWM_MAX_LIMIT       1023    // Límite absoluto (10-bit)
#define HOMING_PWM          300     // PWM fijo para homing
#define NO_MOVE_DETECT_MS   3000    // Si 0 pulsos tras este tiempo → no se mueve
#define TIMEOUT_BACK_MS     10000   // Timeout vuelta (seguridad)
#define PAUSE_AT_END_MS     2000    // Pausa en LS_FIN antes de volver
#define SERIAL_PRINT_MS     500     // Intervalo de progreso

// ============================================================
// Debounce ISR (microsegundos)
// ============================================================
#define DEBOUNCE_US 50000

// ============================================================
// Máximo de registros en tabla
// ============================================================
#define MAX_RECORDS 1024

// ============================================================
// Dirección del motor
// ============================================================
enum MotorDir {
    DIR_STOP     = 0,
    DIR_ADELANTE = 1,
    DIR_ATRAS    = -1
};

// ============================================================
// Estados de la máquina
// ============================================================
enum AppState {
    ST_HOMING,              // Buscando LS_INICIO
    ST_READY,               // En home, listo para siguiente PWM
    ST_FWD_RUNNING,         // Adelante con PWM actual
    ST_PAUSE_AT_END,        // En LS_FIN, pausa 2 s
    ST_BACK_RUNNING,        // Regresando a LS_INICIO
    ST_SHOW_RESULTS,        // Mostrar tabla
    ST_WAIT_REINICIAR       // Esperar comando "reiniciar"
};

// ============================================================
// Registro de cada prueba
// ============================================================
struct TestRecord {
    int      pwm;
    bool     moved;         // ¿Se movió?
    bool     completed;     // ¿Completó ida+vuelta?
    uint32_t pulsesFwd;     // Pulsos en la ida
    uint32_t pulsesBack;    // Pulsos en la vuelta
    unsigned long timeFwdMs;    // Tiempo de ida
    unsigned long timeBackMs;   // Tiempo de vuelta
    float    rpmFwd;        // RPM promedio ida
    float    rpmBack;       // RPM promedio vuelta
};

TestRecord records[MAX_RECORDS];
int recordCount = 0;

// ============================================================
// Variables del encoder
// ============================================================
volatile uint32_t isrPulseCount = 0;
static uint32_t lastReadPulses  = 0;
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
AppState currentState = ST_HOMING;
unsigned long stateEntry = 0;
unsigned long lastPrint  = 0;

// PWM actual del barrido
int currentPWM = PWM_START;

// PWM mínimo y máximo encontrados
int pwmMin = -1;
int pwmMax = -1;

// Mediciones del ciclo actual
uint32_t fwdStartPulses = 0;
uint32_t fwdEndPulses   = 0;
uint32_t backStartPulses = 0;
unsigned long fwdStartTime = 0;
unsigned long fwdEndTime   = 0;
unsigned long backStartTime = 0;

// Flag de stop recibido
bool stopReceived = false;

// Buffer serial
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
// Lectura atómica de pulsos
// ============================================================
uint32_t getPulseCount() {
    noInterrupts();
    uint32_t c = isrPulseCount;
    interrupts();
    return c;
}

// ============================================================
// Resetear pulsos
// ============================================================
void resetPulseCount() {
    noInterrupts();
    isrPulseCount = 0;
    interrupts();
    lastReadPulses = 0;
}

// ============================================================
// Cambiar estado
// ============================================================
void changeState(AppState s) {
    currentState = s;
    stateEntry   = millis();
}

// ============================================================
// Leer línea serial (no bloqueante)
// Retorna true si se completó una línea.
// El contenido queda en serialBuf (null-terminated).
// ============================================================
bool readSerialLine() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialIdx > 0) {
                serialBuf[serialIdx] = '\0';
                serialIdx = 0;
                return true;
            }
        } else if (serialIdx < SERIAL_BUF_SIZE - 1) {
            serialBuf[serialIdx++] = c;
        }
    }
    return false;
}

// ============================================================
// Verificar si el usuario envió "stop" (chequeo pasivo)
// Retorna true si se leyó "stop" de la entrada serial.
// Otros datos se descartan en este contexto.
// ============================================================
bool checkForStop() {
    if (readSerialLine()) {
        // Convertir a minúsculas para comparar
        for (int i = 0; serialBuf[i]; i++) {
            if (serialBuf[i] >= 'A' && serialBuf[i] <= 'Z')
                serialBuf[i] += 32;
        }
        if (strncmp(serialBuf, "stop", 4) == 0) {
            return true;
        }
    }
    return false;
}

// ============================================================
// Registrar resultado de un ciclo
// ============================================================
void addRecord(int pwm, bool moved, bool completed,
               uint32_t pFwd, uint32_t pBack,
               unsigned long tFwd, unsigned long tBack) {
    if (recordCount >= MAX_RECORDS) return;

    TestRecord& r = records[recordCount];
    r.pwm       = pwm;
    r.moved     = moved;
    r.completed = completed;
    r.pulsesFwd  = pFwd;
    r.pulsesBack = pBack;
    r.timeFwdMs  = tFwd;
    r.timeBackMs = tBack;

    if (completed && tFwd > 0) {
        float secFwd = tFwd / 1000.0f;
        float pps = (float)pFwd / secFwd;
        r.rpmFwd = (pps / PULSES_PER_REV) * 60.0f;
    } else {
        r.rpmFwd = 0;
    }

    if (completed && tBack > 0) {
        float secBack = tBack / 1000.0f;
        float pps = (float)pBack / secBack;
        r.rpmBack = (pps / PULSES_PER_REV) * 60.0f;
    } else {
        r.rpmBack = 0;
    }

    recordCount++;
}

// ============================================================
// Imprimir tabla de resultados
// ============================================================
void printResults() {
    Serial.println();
    Serial.println("╔═══════════════════════════════════════════════════════════════════════════════════════════╗");
    Serial.println("║                    RESULTADOS — BARRIDO DE RANGO OPERATIVO PWM                          ║");
    Serial.println("╠═══════════════════════════════════════════════════════════════════════════════════════════╣");

    if (pwmMin > 0) {
        Serial.printf("║  PWM MÍNIMO (primer movimiento completo): %4d                                          ║\n", pwmMin);
    } else {
        Serial.println("║  PWM MÍNIMO: no detectado                                                                ║");
    }
    if (pwmMax > 0) {
        Serial.printf("║  PWM MÁXIMO (detenido por usuario):        %4d                                          ║\n", pwmMax);
    } else {
        Serial.printf("║  PWM MÁXIMO: no definido (último probado:  %4d)                                         ║\n",
                      recordCount > 0 ? records[recordCount - 1].pwm : 0);
    }

    Serial.println("╠═══════════════════════════════════════════════════════════════════════════════════════════╣");
    Serial.printf("║  %-6s | %-4s | %-9s | %-10s | %-10s | %-8s | %-8s | %-9s | %-9s ║\n",
                  "#", "PWM", "¿Mueve?", "T.Ida(s)", "T.Vuelta", "P.Ida", "P.Vuelta", "RPM Ida", "RPM Vuel");
    Serial.println("╠═══════════════════════════════════════════════════════════════════════════════════════════╣");

    for (int i = 0; i < recordCount; i++) {
        TestRecord& r = records[i];
        char moveStr[4];
        if (!r.moved)         strcpy(moveStr, "NO");
        else if (r.completed) strcpy(moveStr, "OK");
        else                  strcpy(moveStr, "SI*");

        if (r.completed) {
            Serial.printf("║  %-6d | %4d | %-9s | %8.3f s | %8.3f s | %8lu | %8lu | %9.1f | %9.1f ║\n",
                          i + 1, r.pwm, moveStr,
                          r.timeFwdMs / 1000.0f, r.timeBackMs / 1000.0f,
                          (unsigned long)r.pulsesFwd, (unsigned long)r.pulsesBack,
                          r.rpmFwd, r.rpmBack);
        } else {
            Serial.printf("║  %-6d | %4d | %-9s | %10s | %10s | %8s | %8s | %9s | %9s ║\n",
                          i + 1, r.pwm, moveStr,
                          "—", "—", "—", "—", "—", "—");
        }
    }

    Serial.println("╚═══════════════════════════════════════════════════════════════════════════════════════════╝");
    Serial.println();

    // Resumen rango operativo
    if (pwmMin > 0 && pwmMax > 0) {
        Serial.println("╔══════════════════════════════════════════╗");
        Serial.println("║       RANGO OPERATIVO DEL MOTOR          ║");
        Serial.println("╠══════════════════════════════════════════╣");
        Serial.printf( "║  PWM mínimo:  %4d  (%4.1f%% del rango)   ║\n", pwmMin, pwmMin * 100.0f / 1023);
        Serial.printf( "║  PWM máximo:  %4d  (%4.1f%% del rango)   ║\n", pwmMax, pwmMax * 100.0f / 1023);
        Serial.printf( "║  Rango útil:  %4d valores               ║\n", pwmMax - pwmMin + 1);
        Serial.println("╚══════════════════════════════════════════╝");
    }
}

// ============================================================
// Iniciar barrido nuevo
// ============================================================
void resetSweep() {
    currentPWM   = PWM_START;
    pwmMin       = -1;
    pwmMax       = -1;
    recordCount  = 0;
    stopReceived = false;
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
    Serial.println("  RANGO OPERATIVO PWM — Barrido incremental");
    Serial.println("  Barrido: PWM 1 → 1023 (incremento de 1)");
    Serial.println("  Comandos: 'stop' = detener y marcar máximo");
    Serial.println("=============================================================");
    Serial.println();

    resetSweep();
    lastPrint = millis();

    // Homing
    if (digitalRead(PIN_LIMIT_START) == HIGH) {
        Serial.println("[OK] Carro en posición de inicio.");
        changeState(ST_READY);
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

    bool startActive = digitalRead(PIN_LIMIT_START) == HIGH;
    bool endActive   = digitalRead(PIN_LIMIT_END)   == HIGH;

    // Consumir flags ISR
    if (limitStartTriggered) limitStartTriggered = false;
    if (limitEndTriggered)   limitEndTriggered   = false;

    switch (currentState) {

    // ---------------------------------------------------------
    // HOMING: ir a LS_INICIO
    // ---------------------------------------------------------
    case ST_HOMING:
        if (startActive) {
            motorFreno();
            Serial.println("[OK] Posición home establecida.");
            changeState(ST_READY);
        }
        if ((now - lastPrint) >= 500) {
            lastPrint = now;
            Serial.println("     ... buscando home ...");
        }
        break;

    // ---------------------------------------------------------
    // READY: en home, iniciar siguiente ciclo de PWM
    // ---------------------------------------------------------
    case ST_READY: {
        if (currentPWM > PWM_MAX_LIMIT || stopReceived) {
            // Fin del barrido
            if (stopReceived && pwmMax < 0) {
                pwmMax = currentPWM > PWM_START ? currentPWM - 1 : currentPWM;
            }
            changeState(ST_SHOW_RESULTS);
            break;
        }

        Serial.println();
        Serial.printf("═══ PWM %d / %d ═══════════════════════════════\n",
                      currentPWM, PWM_MAX_LIMIT);

        // Resetear encoder
        resetPulseCount();

        // Registrar inicio
        fwdStartPulses = 0;
        fwdStartTime   = now;

        Serial.printf("  Moviendo ADELANTE con PWM=%d ...\n", currentPWM);
        motorAdelante(currentPWM);
        changeState(ST_FWD_RUNNING);
        break;
    }

    // ---------------------------------------------------------
    // FWD_RUNNING: adelante, esperando LS_FIN o auto-detección
    // de "no se mueve" (0 pulsos tras 3 s)
    // ---------------------------------------------------------
    case ST_FWD_RUNNING: {
        // Verificar stop del usuario
        if (checkForStop()) {
            motorFreno();
            stopReceived = true;
            pwmMax = currentPWM;
            Serial.printf("\n>> STOP recibido. PWM máximo = %d\n", pwmMax);
            Serial.println("  Volviendo a home...");
            motorAtras(HOMING_PWM);
            changeState(ST_HOMING);
            break;
        }

        if (endActive) {
            motorFreno();
            fwdEndPulses = getPulseCount();
            fwdEndTime   = now;
            unsigned long fwdTime = fwdEndTime - fwdStartTime;

            Serial.printf("  >> LS_FIN detectado. Pulsos: %lu | Tiempo: %.3f s\n",
                          (unsigned long)fwdEndPulses, fwdTime / 1000.0f);

            // Registrar PWM mínimo si es el primer recorrido completo
            if (pwmMin < 0) {
                pwmMin = currentPWM;
                Serial.printf("  ★★★ PWM MÍNIMO DETECTADO: %d ★★★\n", pwmMin);
            }

            Serial.println("  Esperando 2 s antes de regresar...");
            changeState(ST_PAUSE_AT_END);
            break;
        }

        // Auto-detección: si tras 3 s hay 0 pulsos → no se mueve
        if ((now - fwdStartTime) >= NO_MOVE_DETECT_MS && getPulseCount() == 0) {
            motorFreno();
            Serial.printf("  → PWM %d: NO se mueve (0 pulsos en 3 s).\n", currentPWM);
            addRecord(currentPWM, false, false, 0, 0, 0, 0);
            currentPWM++;
            changeState(ST_READY);
            break;
        }

        // Progreso
        if ((now - lastPrint) >= SERIAL_PRINT_MS) {
            lastPrint = now;
            uint32_t p = getPulseCount();
            float elapsed = (now - fwdStartTime) / 1000.0f;
            Serial.printf("  t=%.1f s | pulsos=%lu | pos≈%.1f cm\n",
                          elapsed, (unsigned long)p, p * CM_POR_PULSO);
        }
        break;
    }

    // ---------------------------------------------------------
    // PAUSE_AT_END: en LS_FIN, esperar 2 s
    // ---------------------------------------------------------
    case ST_PAUSE_AT_END: {
        if (checkForStop()) {
            stopReceived = true;
            pwmMax = currentPWM;
            Serial.printf("\n>> STOP recibido. PWM máximo = %d\n", pwmMax);
            motorAtras(HOMING_PWM);
            changeState(ST_HOMING);
            break;
        }

        if ((now - stateEntry) >= PAUSE_AT_END_MS) {
            // Resetear encoder para medir vuelta
            backStartPulses = 0;
            resetPulseCount();
            backStartTime = now;

            Serial.printf("  Regresando ATRÁS con PWM=%d ...\n", currentPWM);
            motorAtras(currentPWM);
            changeState(ST_BACK_RUNNING);
        }
        break;
    }

    // ---------------------------------------------------------
    // BACK_RUNNING: regresando a LS_INICIO
    // ---------------------------------------------------------
    case ST_BACK_RUNNING: {
        if (checkForStop()) {
            motorFreno();
            stopReceived = true;
            pwmMax = currentPWM;
            Serial.printf("\n>> STOP recibido. PWM máximo = %d\n", pwmMax);
            motorAtras(HOMING_PWM);
            changeState(ST_HOMING);
            break;
        }

        if (startActive) {
            motorFreno();
            uint32_t backPulses = getPulseCount();
            unsigned long backTime = now - backStartTime;
            unsigned long fwdTime  = fwdEndTime - fwdStartTime;

            Serial.printf("  >> LS_INICIO detectado. Pulsos vuelta: %lu | Tiempo: %.3f s\n",
                          (unsigned long)backPulses, backTime / 1000.0f);

            // Registrar ciclo completo
            addRecord(currentPWM, true, true,
                      fwdEndPulses, backPulses,
                      fwdTime, backTime);

            Serial.printf("  ✓ Ciclo completo PWM=%d\n", currentPWM);

            currentPWM++;
            changeState(ST_READY);
            break;
        }

        // Timeout de seguridad para la vuelta
        if ((now - backStartTime) >= TIMEOUT_BACK_MS) {
            motorFreno();
            Serial.printf("  [!!] Timeout vuelta PWM=%d. Haciendo homing forzado.\n", currentPWM);

            uint32_t backPulses = getPulseCount();
            unsigned long backTime = now - backStartTime;
            unsigned long fwdTime  = fwdEndTime - fwdStartTime;

            addRecord(currentPWM, true, true,
                      fwdEndPulses, backPulses,
                      fwdTime, backTime);

            currentPWM++;
            motorAtras(HOMING_PWM);
            changeState(ST_HOMING);
            break;
        }

        if ((now - lastPrint) >= SERIAL_PRINT_MS) {
            lastPrint = now;
            uint32_t p = getPulseCount();
            float elapsed = (now - backStartTime) / 1000.0f;
            Serial.printf("  regresando... t=%.1f s | pulsos=%lu\n",
                          elapsed, (unsigned long)p);
        }
        break;
    }

    // ---------------------------------------------------------
    // SHOW_RESULTS: imprimir tabla
    // ---------------------------------------------------------
    case ST_SHOW_RESULTS: {
        printResults();
        Serial.println();
        Serial.println("Enviar 'reiniciar' para repetir el barrido completo.");
        Serial.print(">> ");
        changeState(ST_WAIT_REINICIAR);
        break;
    }

    // ---------------------------------------------------------
    // WAIT_REINICIAR: esperar comando
    // ---------------------------------------------------------
    case ST_WAIT_REINICIAR: {
        if (readSerialLine()) {
            for (int i = 0; serialBuf[i]; i++) {
                if (serialBuf[i] >= 'A' && serialBuf[i] <= 'Z')
                    serialBuf[i] += 32;
            }
            if (strncmp(serialBuf, "reiniciar", 9) == 0) {
                Serial.println("\n>> Reiniciando barrido...\n");
                resetSweep();
                // Asegurar home
                if (startActive) {
                    changeState(ST_READY);
                } else {
                    motorAtras(HOMING_PWM);
                    changeState(ST_HOMING);
                }
            } else {
                Serial.println("  Comando no reconocido. Enviar 'reiniciar'.");
                Serial.print(">> ");
            }
        }
        break;
    }
    }
}
