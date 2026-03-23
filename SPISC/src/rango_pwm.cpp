/*
 * ============================================================
 *  RANGO OPERATIVO PWM — Barrido de velocidad (WiFi + MQTT)
 *  Proyecto Péndulo Invertido
 * ============================================================
 *
 *  Adaptación de scripts/rango_operativo_pwm.ino para
 *  comunicación WiFi + MQTT en lugar de Serial.
 *
 *  Publica telemetría en tiempo real y resultados al final
 *  del barrido. Recibe comandos "start", "stop" y "reiniciar"
 *  por MQTT (tópico pendulum/cmd/sweep).
 *
 *  Hardware:
 *    - ESP32-WROOM-32 DevKitC
 *    - Motor DC + BTS7960 H-Bridge
 *    - Encoder HC-020K (40 PPR)
 *    - Limit Switch XV-152-1C25 x2 (NC + Pull-Up)
 *
 *  Pines:
 *    Motor:   R_EN→26, L_EN→27, R_PWM→12, L_PWM→14
 *    Encoder: OUT→16
 *    LS:      INICIO→32, FIN→33
 * ============================================================
 */

#include <Arduino.h>
#include <MqttBridge.h>
#include <wifi_mqtt_config.h>

// ══════════════════════════════════════════════════
// Pines
// ══════════════════════════════════════════════════
#define PIN_R_EN   26
#define PIN_L_EN   27
#define PIN_R_PWM  12
#define PIN_L_PWM  14

#define PIN_ENCODER      16
#define PIN_LIMIT_START  32
#define PIN_LIMIT_END    33

// ══════════════════════════════════════════════════
// PWM (LEDC)
// ══════════════════════════════════════════════════
#define PWM_FREQ        25000
#define PWM_RESOLUTION  10      // 0-1023
#define LEDC_CH_R       0
#define LEDC_CH_L       1

// ══════════════════════════════════════════════════
// Encoder
// ══════════════════════════════════════════════════
#define PULSES_PER_REV  40
#define CM_POR_PULSO    0.0504f

// ══════════════════════════════════════════════════
// Parámetros del barrido
// ══════════════════════════════════════════════════
#define PWM_START           400
#define PWM_MAX_LIMIT       1023
#define HOMING_PWM          300
#define NO_MOVE_DETECT_MS   3000
#define TIMEOUT_BACK_MS     10000
#define PAUSE_AT_END_MS     2000
#define SETTLE_MS           1000     // presionar contra tope tras LS
#define PAUSE_AT_START_MS   2000     // pausa en inicio antes del siguiente ciclo

// ══════════════════════════════════════════════════
// Intervalos de publicación (ms)
// ══════════════════════════════════════════════════
#define TELEMETRY_INTERVAL_MS  200   // 5 Hz
#define STATUS_INTERVAL_MS     500

// ══════════════════════════════════════════════════
// Debounce ISR (µs)
// ══════════════════════════════════════════════════
#define DEBOUNCE_US  50000

// ══════════════════════════════════════════════════
// Máximo registros
// ══════════════════════════════════════════════════
#define MAX_RECORDS 1024

// ══════════════════════════════════════════════════
// Tipos
// ══════════════════════════════════════════════════
enum MotorDir { DIR_STOP = 0, DIR_ADELANTE = 1, DIR_ATRAS = -1 };

enum AppState {
    ST_IDLE,               // Esperando comando "start"
    ST_HOMING,
    ST_READY,
    ST_FWD_RUNNING,
    ST_SETTLE_END,         // presionando contra tope fin
    ST_PAUSE_AT_END,
    ST_BACK_RUNNING,
    ST_SETTLE_START,       // presionando contra tope inicio
    ST_PAUSE_AT_START,     // pausa en inicio
    ST_SHOW_RESULTS,
    ST_WAIT_REINICIAR
};

struct TestRecord {
    int      pwm;
    bool     moved;
    bool     completed;
    uint32_t pulsesFwd;
    uint32_t pulsesBack;
    unsigned long timeFwdMs;
    unsigned long timeBackMs;
    float    rpmFwd;
    float    rpmBack;
};

// ══════════════════════════════════════════════════
// Variables globales
// ══════════════════════════════════════════════════
static MqttBridge mqtt;

static TestRecord records[MAX_RECORDS];
static int recordCount = 0;

// Encoder ISR
static volatile uint32_t isrPulseCount = 0;
static volatile int32_t  globalPosition = 0;   // posición con signo
static volatile MotorDir motorDirection = DIR_STOP;

// Limit switch ISR flags
static volatile bool limitStartTriggered = false;
static volatile bool limitEndTriggered   = false;
static volatile unsigned long lastISR_start = 0;
static volatile unsigned long lastISR_end   = 0;

// Estado
static AppState currentState = ST_IDLE;
static unsigned long stateEntry = 0;

// Publicación
static unsigned long lastTelemetry = 0;
static unsigned long lastStatus    = 0;

// Barrido
static int currentPWM = PWM_START;
static int pwmMin     = -1;
static int pwmMax     = -1;

// Mediciones del ciclo actual
static uint32_t fwdEndPulses     = 0;
static unsigned long fwdStartTime = 0;
static unsigned long fwdEndTime   = 0;
static unsigned long backStartTime = 0;
static bool fromHoming = false;
static uint32_t backEndPulses = 0;
static unsigned long backEndTime = 0;

// Flags de comando MQTT
static volatile bool cmdStart     = false;
static volatile bool cmdStop      = false;
static volatile bool cmdReiniciar = false;
static bool stopPending           = false;  // tras homing por stop → resultados

// JSON buffer
static char jsonBuf[512];

// ══════════════════════════════════════════════════
// ISRs
// ══════════════════════════════════════════════════
void IRAM_ATTR encoder_isr() {
    isrPulseCount++;
    if (motorDirection == DIR_ADELANTE)
        globalPosition++;
    else if (motorDirection == DIR_ATRAS)
        globalPosition--;
}

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

// ══════════════════════════════════════════════════
// Motor
// ══════════════════════════════════════════════════
static void motorAdelante(int pwm) {
    motorDirection = DIR_ADELANTE;
    ledcWrite(LEDC_CH_L, 0);
    ledcWrite(LEDC_CH_R, pwm);
}

static void motorAtras(int pwm) {
    motorDirection = DIR_ATRAS;
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, pwm);
}

static void motorFreno() {
    motorDirection = DIR_STOP;
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, 0);
}

// ══════════════════════════════════════════════════
// Encoder helpers
// ══════════════════════════════════════════════════
static uint32_t getPulseCount() {
    noInterrupts();
    uint32_t c = isrPulseCount;
    interrupts();
    return c;
}

static void resetPulseCount() {
    noInterrupts();
    isrPulseCount = 0;
    interrupts();
}

// ══════════════════════════════════════════════════
// Estado helpers
// ══════════════════════════════════════════════════
static const char* stateToStr(AppState s) {
    switch (s) {
        case ST_IDLE:           return "IDLE";
        case ST_HOMING:         return "HOMING";
        case ST_READY:          return "READY";
        case ST_FWD_RUNNING:    return "FWD_RUNNING";
        case ST_SETTLE_END:     return "SETTLE_END";
        case ST_PAUSE_AT_END:   return "PAUSE_AT_END";
        case ST_BACK_RUNNING:   return "BACK_RUNNING";
        case ST_SETTLE_START:   return "SETTLE_START";
        case ST_PAUSE_AT_START: return "PAUSE_AT_START";
        case ST_SHOW_RESULTS:   return "SHOW_RESULTS";
        case ST_WAIT_REINICIAR: return "WAIT_REINICIAR";
        default:                return "UNKNOWN";
    }
}

static void changeState(AppState s) {
    currentState = s;
    stateEntry   = millis();
}

// ══════════════════════════════════════════════════
// Registrar resultado de un ciclo
// ══════════════════════════════════════════════════
static void addRecord(int pwm, bool moved, bool completed,
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
        float sec = tFwd / 1000.0f;
        r.rpmFwd = ((float)pFwd / sec / PULSES_PER_REV) * 60.0f;
    } else {
        r.rpmFwd = 0;
    }

    if (completed && tBack > 0) {
        float sec = tBack / 1000.0f;
        r.rpmBack = ((float)pBack / sec / PULSES_PER_REV) * 60.0f;
    } else {
        r.rpmBack = 0;
    }

    recordCount++;
}

// ══════════════════════════════════════════════════
// Publicaciones MQTT
// ══════════════════════════════════════════════════

static int32_t getGlobalPosition() {
    noInterrupts();
    int32_t p = globalPosition;
    interrupts();
    return p;
}

/** Publica telemetría periódica (5 Hz). */
static void publishTelemetry() {
    uint32_t pulses = getPulseCount();
    float posCm = getGlobalPosition() * CM_POR_PULSO;
    float rpmInst = 0;

    // RPM instantánea basada en pulsos recientes (simplificada)
    static uint32_t prevPulses = 0;
    static unsigned long prevTime = 0;
    unsigned long now = millis();
    if (prevTime > 0 && (now - prevTime) > 0) {
        uint32_t dp = (pulses >= prevPulses) ? (pulses - prevPulses) : 0;
        float dt = (now - prevTime) / 1000.0f;
        rpmInst = ((float)dp / dt / PULSES_PER_REV) * 60.0f;
    }
    prevPulses = pulses;
    prevTime = now;

    snprintf(jsonBuf, sizeof(jsonBuf),
        "{\"pwm\":%d,\"rpm\":%.1f,\"posicion\":%.2f,\"pulsos\":%lu,"
        "\"direccion\":%d,\"modo\":\"SWEEP_PWM\","
        "\"sweep\":{\"currentPwm\":%d,\"pwmMin\":%d,\"pwmMax\":%d,"
        "\"recordsCount\":%d,\"totalSteps\":%d}}",
        (motorDirection == DIR_STOP) ? 0 : currentPWM,
        rpmInst, posCm, (unsigned long)pulses,
        (int)motorDirection,
        currentPWM, pwmMin, pwmMax,
        recordCount, PWM_MAX_LIMIT);

    mqtt.publish(TOPIC_TELEMETRY, jsonBuf);
}

/** Publica estado (cambio de estado y periódico). */
static void publishStatus() {
    bool lsStart = digitalRead(PIN_LIMIT_START) == HIGH;
    bool lsEnd   = digitalRead(PIN_LIMIT_END) == HIGH;

    snprintf(jsonBuf, sizeof(jsonBuf),
        "{\"estado\":\"%s\",\"modo\":\"SWEEP_PWM\","
        "\"ls_inicio\":%s,\"ls_fin\":%s,"
        "\"wifi_rssi\":%d}",
        stateToStr(currentState),
        lsStart ? "true" : "false",
        lsEnd   ? "true" : "false",
        mqtt.getWifiRssi());

    mqtt.publish(TOPIC_STATUS, jsonBuf);
}

/** Publica limits (cambio de estado de switch). */
static void publishLimits(bool lsStart, bool lsEnd) {
    snprintf(jsonBuf, sizeof(jsonBuf),
        "{\"inicio\":%s,\"fin\":%s}",
        lsStart ? "true" : "false",
        lsEnd   ? "true" : "false");  
    mqtt.publish(TOPIC_LIMITS, jsonBuf);
}

/** Publica todos los resultados al finalizar el barrido. */
static void publishResults() {
    // Publicar resumen primero
    snprintf(jsonBuf, sizeof(jsonBuf),
        "{\"type\":\"summary\",\"pwmMin\":%d,\"pwmMax\":%d,"
        "\"totalRecords\":%d,\"rangoUtil\":%d}",
        pwmMin, pwmMax, recordCount,
        (pwmMin > 0 && pwmMax > 0) ? (pwmMax - pwmMin + 1) : 0);
    mqtt.publish(TOPIC_SWEEP_RESULTS, jsonBuf);
    delay(50);

    // Publicar cada registro individual
    for (int i = 0; i < recordCount; i++) {
        TestRecord& r = records[i];
        snprintf(jsonBuf, sizeof(jsonBuf),
            "{\"type\":\"record\",\"idx\":%d,\"pwm\":%d,"
            "\"moved\":%s,\"completed\":%s,"
            "\"pulsesFwd\":%lu,\"pulsesBack\":%lu,"
            "\"timeFwdMs\":%lu,\"timeBackMs\":%lu,"
            "\"rpmFwd\":%.1f,\"rpmBack\":%.1f}",
            i, r.pwm,
            r.moved     ? "true" : "false",
            r.completed ? "true" : "false",
            (unsigned long)r.pulsesFwd, (unsigned long)r.pulsesBack,
            r.timeFwdMs, r.timeBackMs,
            r.rpmFwd, r.rpmBack);
        mqtt.publish(TOPIC_SWEEP_RESULTS, jsonBuf);

        // Yield para no saturar el buffer MQTT
        mqtt.loop();
        delay(20);
    }

    // Señal de fin
    snprintf(jsonBuf, sizeof(jsonBuf),
        "{\"type\":\"done\",\"totalRecords\":%d}", recordCount);
    mqtt.publish(TOPIC_SWEEP_RESULTS, jsonBuf);
}

// ══════════════════════════════════════════════════
// Callback MQTT — Comandos recibidos
// ══════════════════════════════════════════════════
static void onMqttMessage(const char* topic, const char* payload) {
    // Comando de parada de emergencia (tópico dedicado)
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        cmdStop = true;
        return;
    }

    // Comando sweep (start / stop / reiniciar)
    if (strcmp(topic, TOPIC_CMD_SWEEP) != 0) return;

    // Parseo simplificado: buscar "action":"..."
    const char* actionKey = strstr(payload, "\"action\"");
    if (!actionKey) return;

    if (strstr(actionKey, "\"start\""))     cmdStart = true;
    else if (strstr(actionKey, "\"stop\"")) cmdStop = true;
    else if (strstr(actionKey, "\"reiniciar\"")) cmdReiniciar = true;
}

// ══════════════════════════════════════════════════
// Reset del barrido
// ══════════════════════════════════════════════════
static void resetSweep() {
    currentPWM   = PWM_START;
    pwmMin       = -1;
    pwmMax       = -1;
    recordCount  = 0;
    cmdStop      = false;
    cmdStart     = false;
    cmdReiniciar = false;
    stopPending  = false;
}

// ══════════════════════════════════════════════════
// Setup
// ══════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("[rango_pwm] Iniciando...");

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

    // WiFi + MQTT
    mqtt.onMessage(onMqttMessage);
    if (!mqtt.begin()) {
        Serial.println("[rango_pwm] ERROR: WiFi no conectó.");
        // Continuar de todas formas, la lib reintenta
    }
    mqtt.subscribe(TOPIC_CMD_SWEEP);
    mqtt.subscribe(TOPIC_CMD_STOP);

    resetSweep();
    globalPosition = 0;
    changeState(ST_IDLE);
    Serial.println("[rango_pwm] Listo. Esperando comando 'start' por MQTT...");
}

// ══════════════════════════════════════════════════
// Loop
// ══════════════════════════════════════════════════
void loop() {
    mqtt.loop();

    unsigned long now = millis();
    bool startActive = digitalRead(PIN_LIMIT_START) == HIGH;
    bool endActive   = digitalRead(PIN_LIMIT_END)   == HIGH;

    // Publicar estado de limit switches si cambiaron
    static bool prevLsStart = false, prevLsEnd = false;
    if (startActive != prevLsStart || endActive != prevLsEnd) {
        publishLimits(startActive, endActive);
        prevLsStart = startActive;
        prevLsEnd = endActive;
    }

    // Consumir flags ISR
    if (limitStartTriggered) limitStartTriggered = false;
    if (limitEndTriggered)   limitEndTriggered = false;

    // Publicar telemetría periódica (solo durante barrido)
    if (currentState != ST_IDLE && currentState != ST_WAIT_REINICIAR) {
        if ((now - lastTelemetry) >= TELEMETRY_INTERVAL_MS) {
            lastTelemetry = now;
            publishTelemetry();
        }
    }

    // Publicar estado periódico
    if ((now - lastStatus) >= STATUS_INTERVAL_MS) {
        lastStatus = now;
        publishStatus();
    }

    // Parada de emergencia global
    if (cmdStop) {
        cmdStop = false;  // siempre limpiar la bandera
        if (currentState != ST_IDLE && currentState != ST_SHOW_RESULTS
            && currentState != ST_WAIT_REINICIAR) {
            motorFreno();
            if (pwmMax < 0) {
                pwmMax = currentPWM > PWM_START ? currentPWM - 1 : currentPWM;
            }
            Serial.printf("[rango_pwm] STOP. PWM máximo = %d\n", pwmMax);
            // Volver a home antes de mostrar resultados
            motorAtras(HOMING_PWM);
            changeState(ST_HOMING);
            fromHoming = true;
            stopPending = true;  // tras homing → resultados
            return;
        } else {
            motorFreno();
            Serial.println("[rango_pwm] STOP en estado inactivo, motor frenado");
        }
    }

    switch (currentState) {

    // ─────────────────────────────────────────────
    // IDLE: esperando comando "start" desde la HMI
    // ─────────────────────────────────────────────
    case ST_IDLE:
        if (cmdStart) {
            cmdStart = false;
            Serial.println("[rango_pwm] Comando START recibido.");
            resetSweep();
            // Verificar home
            if (startActive) {
                changeState(ST_READY);
            } else {
                Serial.println("[rango_pwm] Haciendo homing...");
                motorAtras(HOMING_PWM);
                changeState(ST_HOMING);
            }
            publishStatus();
        }
        break;

    // ─────────────────────────────────────────────
    // HOMING: ir a LS_INICIO
    // ─────────────────────────────────────────────
    case ST_HOMING:
        if (startActive) {
            fromHoming = true;
            changeState(ST_SETTLE_START);
            publishStatus();
        }
        break;

    // ─────────────────────────────────────────────
    // READY: en home, iniciar siguiente ciclo PWM
    // ─────────────────────────────────────────────
    case ST_READY: {
        if (currentPWM > PWM_MAX_LIMIT) {
            changeState(ST_SHOW_RESULTS);
            break;
        }

        Serial.printf("[rango_pwm] PWM %d / %d\n", currentPWM, PWM_MAX_LIMIT);
        resetPulseCount();
        fwdStartTime = now;

        motorAdelante(currentPWM);
        changeState(ST_FWD_RUNNING);
        publishStatus();
        break;
    }

    // ─────────────────────────────────────────────
    // FWD_RUNNING: adelante, esperando LS_FIN o no-move
    // ─────────────────────────────────────────────
    case ST_FWD_RUNNING: {
        if (endActive) {
            fwdEndPulses = getPulseCount();
            fwdEndTime   = now;

            if (pwmMin < 0) {
                pwmMin = currentPWM;
                Serial.printf("[rango_pwm] PWM MÍNIMO: %d\n", pwmMin);
            }

            // Mantener motor adelante para presionar contra tope
            changeState(ST_SETTLE_END);
            publishStatus();
            break;
        }

        // No se mueve: menos de 3 pulsos tras NO_MOVE_DETECT_MS (umbral contra ruido)
        if ((now - fwdStartTime) >= NO_MOVE_DETECT_MS && getPulseCount() < 3) {
            motorFreno();
            addRecord(currentPWM, false, false, 0, 0, 0, 0);
            currentPWM++;
            changeState(ST_READY);
            break;
        }
        break;
    }

    // ─────────────────────────────────────────────
    // SETTLE_END: presionar contra tope fin por SETTLE_MS
    // ─────────────────────────────────────────────
    case ST_SETTLE_END: {
        if ((now - stateEntry) >= SETTLE_MS) {
            motorFreno();
            changeState(ST_PAUSE_AT_END);
            publishStatus();
        }
        break;
    }

    // ─────────────────────────────────────────────
    // PAUSE_AT_END: en LS_FIN, pausa antes de volver
    // ─────────────────────────────────────────────
    case ST_PAUSE_AT_END: {
        if ((now - stateEntry) >= PAUSE_AT_END_MS) {
            resetPulseCount();
            backStartTime = now;

            motorAtras(currentPWM);
            changeState(ST_BACK_RUNNING);
            publishStatus();
        }
        break;
    }

    // ─────────────────────────────────────────────
    // BACK_RUNNING: regresando a LS_INICIO
    // ─────────────────────────────────────────────
    case ST_BACK_RUNNING: {
        if (startActive) {
            backEndPulses = getPulseCount();
            backEndTime = now;
            fromHoming = false;
            // Mantener motor atrás para presionar contra tope
            changeState(ST_SETTLE_START);
            publishStatus();
            break;
        }

        // Timeout seguridad vuelta
        if ((now - backStartTime) >= TIMEOUT_BACK_MS) {
            motorFreno();
            Serial.printf("[rango_pwm] Timeout vuelta PWM=%d, homing forzado.\n", currentPWM);

            uint32_t backPulses = getPulseCount();
            unsigned long backTime = now - backStartTime;
            unsigned long fwdTime  = fwdEndTime - fwdStartTime;

            addRecord(currentPWM, true, true,
                      fwdEndPulses, backPulses,
                      fwdTime, backTime);

            currentPWM++;
            fromHoming = true;
            motorAtras(HOMING_PWM);
            changeState(ST_HOMING);
            break;
        }
        break;
    }

    // ─────────────────────────────────────────────
    // SETTLE_START: presionar contra tope inicio por SETTLE_MS
    // ─────────────────────────────────────────────
    case ST_SETTLE_START: {
        if ((now - stateEntry) >= SETTLE_MS) {
            motorFreno();
            noInterrupts(); globalPosition = 0; interrupts();
            resetPulseCount();

            if (fromHoming) {
                Serial.println("[rango_pwm] Home alcanzado.");
                if (stopPending) {
                    stopPending = false;
                    changeState(ST_SHOW_RESULTS);
                } else {
                    changeState(ST_READY);
                }
            } else {
                unsigned long fwdTime = fwdEndTime - fwdStartTime;
                unsigned long backTime = backEndTime - backStartTime;
                addRecord(currentPWM, true, true,
                          fwdEndPulses, backEndPulses,
                          fwdTime, backTime);
                Serial.printf("[rango_pwm] Ciclo completo PWM=%d\n", currentPWM);
                currentPWM++;
                changeState(ST_PAUSE_AT_START);
            }
            publishStatus();
        }
        break;
    }

    // ─────────────────────────────────────────────
    // PAUSE_AT_START: pausa en inicio antes del siguiente ciclo
    // ─────────────────────────────────────────────
    case ST_PAUSE_AT_START: {
        if ((now - stateEntry) >= PAUSE_AT_START_MS) {
            changeState(ST_READY);
            publishStatus();
        }
        break;
    }

    // ─────────────────────────────────────────────
    // SHOW_RESULTS: publicar tabla de resultados
    // ─────────────────────────────────────────────
    case ST_SHOW_RESULTS: {
        Serial.printf("[rango_pwm] Publicando resultados (%d registros)...\n", recordCount);
        publishResults();
        changeState(ST_WAIT_REINICIAR);
        publishStatus();
        break;
    }

    // ─────────────────────────────────────────────
    // WAIT_REINICIAR: esperar comando reiniciar
    // ─────────────────────────────────────────────
    case ST_WAIT_REINICIAR: {
        if (cmdReiniciar || cmdStart) {
            cmdReiniciar = false;
            cmdStart = false;
            Serial.println("[rango_pwm] Reiniciando barrido...");
            resetSweep();
            if (startActive) {
                changeState(ST_READY);
            } else {
                motorAtras(HOMING_PWM);
                changeState(ST_HOMING);
            }
            publishStatus();
        }
        break;
    }
    } // switch
}
