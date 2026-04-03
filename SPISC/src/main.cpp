/*
 * ═══════════════════════════════════════════════════════════════
 *  main.cpp — Orquestador Dual-Core del firmware SPISC.
 *
 *  ARQUITECTURA DUAL-CORE:
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │  CORE 0 — Sensores y Control (500 Hz, crítico)             │
 *  │    - Lectura de sensores cada 2ms                          │
 *  │    - Procesamiento de comandos MQTT (desde cola)           │
 *  │    - Verificación de seguridad                             │
 *  │    - Ejecución de modos de control                         │
 *  └─────────────────────────────────────────────────────────────┘
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │  CORE 1 — Comunicación (flexible, no crítico)              │
 *  │    - MQTT keep-alive y reconexión                          │
 *  │    - Publicación de telemetría (5 Hz)                      │
 *  │    - Publicación de estado (2 Hz)                          │
 *  │    - Actualización de OLED (2 Hz)                          │
 *  └─────────────────────────────────────────────────────────────┘
 *
 *  Modos soportados: SWEEP_PWM, OPEN_LOOP (extensible).
 * ═══════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// FreeRTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Módulos compartidos
#include "hardware_pins.h"
#include "telemetry_data.h"
#include "motor_control.h"
#include "encoder_ir.h"
#include "limit_switches.h"
#include "sensors_i2c.h"
#include "mqtt_comm.h"
#include "display_oled.h"
#include "safety.h"
#include <wifi_mqtt_config.h>

// Modos
#include "modes/rango_pwm_mode.h"
#include "modes/lazo_abierto_mode.h"
#include "modes/pid_posicion_mode.h"

// ══════════════════════════════════════════════════
// Constantes Dual-Core
// ══════════════════════════════════════════════════
#define SENSORS_TASK_PERIOD_MS   1      // 1000 Hz. | Antes 500 Hz
#define SENSORS_TASK_STACK    4096      // bytes
#define SENSORS_TASK_PRIORITY    5      // Alta prioridad

#define COMM_TASK_STACK       8192      // bytes (JSON + WiFi buffers)
#define COMM_TASK_PRIORITY       3      // Prioridad media
#define COMM_TASK_PERIOD_MS     10      // Yield cada 10ms

#define WATCHDOG_TIMEOUT_S       5      // Segundos

// ══════════════════════════════════════════════════
// Handles de tareas FreeRTOS
// ══════════════════════════════════════════════════
static TaskHandle_t taskSensorsHandle = NULL;
static TaskHandle_t taskCommHandle    = NULL;

// ══════════════════════════════════════════════════
// Datos compartidos (protegidos por mutex)
// ══════════════════════════════════════════════════
SemaphoreHandle_t telemetryMutex = NULL;  // Definición del extern de telemetry_data.h
static TelemetryPacket sharedTelemetry = {};
static FirmwareMode activeMode = MODE_IDLE;

// ══════════════════════════════════════════════════
// Auto-calibración de posición por limit switches
// ══════════════════════════════════════════════════
static bool _prevLsStart = false;
static bool _prevLsEnd   = false;

// Forward declarations
static const char* getActiveModeStateStr();
static void buildTelemetry(TelemetryPacket& pkt);
static void switchMode(FirmwareMode newMode);
static void onMqttCommand(const char* topic, const char* payload);

// ══════════════════════════════════════════════════
// Construir paquete de telemetría
// ══════════════════════════════════════════════════
static void buildTelemetry(TelemetryPacket& pkt) {
    // Encoder IR
    pkt.pulsos_ir = EncoderIR::getPulseCount();
    pkt.vueltas_ir = EncoderIR::getVueltas();
    pkt.rps_ir    = EncoderIR::getRPS();
    pkt.rpm_ir    = EncoderIR::getRPM();
    pkt.pos_ir_cm = EncoderIR::getPositionCm();

    // AS5600 posición carro (magnético)
    pkt.rps_mag    = SensorsI2C::getRpsMag();
    pkt.rpm_mag    = SensorsI2C::getRpmMag();
    pkt.pos_mag_cm = SensorsI2C::getPosMagCm();

    // AS5600 ángulo péndulo
    pkt.angle_pend   = SensorsI2C::getAnglePend();
    pkt.vel_ang_pend = SensorsI2C::getAngularVelPend();

    // MPU6050
    pkt.mpu_ax    = SensorsI2C::getAccelX();
    pkt.mpu_ay    = SensorsI2C::getAccelY();
    pkt.mpu_az    = SensorsI2C::getAccelZ();
    pkt.mpu_gx    = SensorsI2C::getGyroX();
    pkt.mpu_gy    = SensorsI2C::getGyroY();
    pkt.mpu_gz    = SensorsI2C::getGyroZ();
    pkt.mpu_pitch = SensorsI2C::getPitch();
    pkt.mpu_roll  = SensorsI2C::getRoll();

    // Motor
    pkt.pwm       = Motor::getCurrentPWM();
    pkt.direction = Motor::getDirection();

    // Limit switches
    pkt.ls_start  = LimitSwitches::isStartActive();
    pkt.ls_end    = LimitSwitches::isEndActive();

    // Metadata
    pkt.ts       = millis();
    pkt.modo     = activeMode;
    pkt.ol_state = getActiveModeStateStr();

    // Control PID Posición (valores de PidPosicionMode)
    pkt.pid_error      = PidPosicionMode::getError();
    pkt.pid_integral   = PidPosicionMode::getIntegral();
    pkt.pid_derivada   = PidPosicionMode::getDerivada();
    pkt.pid_output     = PidPosicionMode::getOutput();
    pkt.pid_setpoint   = PidPosicionMode::getSetpoint();
    pkt.pid_kp         = PidPosicionMode::getKp();
    pkt.pid_ki         = PidPosicionMode::getKi();
    pkt.pid_kd         = PidPosicionMode::getKd();
    pkt.pid_zona_seg   = PidPosicionMode::getZonaSeguridad();
    pkt.pid_tolerancia = PidPosicionMode::getTolerancia();
    pkt.pid_output_sat = PidPosicionMode::getOutputSaturated();
    pkt.pid_sensor_type = PidPosicionMode::getSensorType();
    pkt.pid_pwm_min    = PidPosicionMode::getPwmMin();
    pkt.pid_pwm_max    = PidPosicionMode::getPwmMax();

    // Flag de telemetría rápida (20 Hz) cuando PID está activo
    pkt.telemetry_fast = (activeMode == MODE_PID_POS) && PidPosicionMode::isActive();
}

// ══════════════════════════════════════════════════
// Obtener string del estado del modo activo
// ══════════════════════════════════════════════════
static const char* getActiveModeStateStr() {
    switch (activeMode) {
        case MODE_SWEEP_PWM:  return RangoPwmMode::getStateStr();
        case MODE_OPEN_LOOP:  return LazoAbiertoMode::getStateStr();
        case MODE_PID_POS:    return PidPosicionMode::getStateStr();
        default:              return "IDLE";
    }
}

// ══════════════════════════════════════════════════
// Cambiar modo activo
// ══════════════════════════════════════════════════
static void switchMode(FirmwareMode newMode) {
    if (newMode == activeMode) return;

    // Frenar motor al cambiar de modo
    Motor::freno();

    activeMode = newMode;
    Serial.printf("[main] Cambio de modo → %s\n", firmwareModeToStr(newMode));

    switch (newMode) {
        case MODE_SWEEP_PWM:  RangoPwmMode::begin(); break;
        case MODE_OPEN_LOOP:  LazoAbiertoMode::begin(); break;
        case MODE_PID_POS:    PidPosicionMode::begin(); break;
        default: break;
    }
}

// ══════════════════════════════════════════════════
// Callback MQTT (ejecutado en Core 0 via processCommandQueue)
// ══════════════════════════════════════════════════
static void onMqttCommand(const char* topic, const char* payload) {
    Safety::markMqttActivity();

    // Parada de emergencia global
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        Motor::freno();
        switch (activeMode) {
            case MODE_SWEEP_PWM:  RangoPwmMode::onCommand(topic, payload); break;
            case MODE_OPEN_LOOP:  LazoAbiertoMode::onCommand(topic, payload); break;
            case MODE_PID_POS:    PidPosicionMode::onCommand(topic, payload); break;
            default: break;
        }
        return;
    }

    // Cambio de modo
    if (strcmp(topic, TOPIC_CMD_MODE) == 0) {
        const char* modeKey = strstr(payload, "\"mode\"");
        if (modeKey) {
            FirmwareMode m = MODE_IDLE;
            if (strstr(modeKey, "\"SWEEP_PWM\""))   m = MODE_SWEEP_PWM;
            else if (strstr(modeKey, "\"OPEN_LOOP\"")) m = MODE_OPEN_LOOP;
            else if (strstr(modeKey, "\"PID_POS\""))   m = MODE_PID_POS;
            else if (strstr(modeKey, "\"IDLE\""))      m = MODE_IDLE;
            switchMode(m);
        }
        return;
    }

    // Calibración
    if (strcmp(topic, TOPIC_CMD_CALIBRATE) == 0) {
        const char* typeKey = strstr(payload, "\"type\"");
        if (typeKey && strstr(typeKey, "\"position\"")) {
            SensorsI2C::resetMagPosition();
        } else if (typeKey && strstr(typeKey, "\"mpu\"")) {
            SensorsI2C::calibrateMPU();
        } else if (typeKey && strstr(typeKey, "\"full\"")) {
            SensorsI2C::calibrateAngle();
            SensorsI2C::resetMagPosition();
            SensorsI2C::calibrateMPU();
        } else {
            SensorsI2C::calibrateAngle();
        }
        return;
    }

    // Debug encoder IR
    if (strcmp(topic, TOPIC_CMD_DEBUG) == 0) {
        const char* encKey = strstr(payload, "\"encoder\"");
        if (encKey) {
            if (strstr(encKey, "\"on\"")) {
                EncoderIR::enableDebug();
            } else if (strstr(encKey, "\"off\"")) {
                EncoderIR::disableDebug();
            }
        }
        return;
    }

    // Auto-switch de modo segun topico del comando
    if (strcmp(topic, TOPIC_CMD_SWEEP) == 0 && activeMode != MODE_SWEEP_PWM) {
        switchMode(MODE_SWEEP_PWM);
    } else if (strcmp(topic, TOPIC_CMD_OPENLOOP) == 0 && activeMode != MODE_OPEN_LOOP) {
        switchMode(MODE_OPEN_LOOP);
    } else if (strcmp(topic, TOPIC_CMD_PID_POS) == 0 && activeMode != MODE_PID_POS) {
        switchMode(MODE_PID_POS);
    }

    // Enrutar al modo activo
    switch (activeMode) {
        case MODE_SWEEP_PWM:  RangoPwmMode::onCommand(topic, payload); break;
        case MODE_OPEN_LOOP:  LazoAbiertoMode::onCommand(topic, payload); break;
        case MODE_PID_POS:    PidPosicionMode::onCommand(topic, payload); break;
        default: break;
    }
}

// ══════════════════════════════════════════════════════════════════
// TAREA CORE 0: Sensores y Control (500 Hz, crítico)
// ══════════════════════════════════════════════════════════════════
void taskSensorsAndControl(void* pvParameters) {
    Serial.println("[Core0] Tarea de sensores iniciada");
    
    // Inicializar watchdog para esta tarea
    Safety::initHardwareWatchdog(WATCHDOG_TIMEOUT_S);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSORS_TASK_PERIOD_MS);

    for (;;) {
        // 0. Alimentar watchdog PRIMERO (evita timeout si I2C se bloquea)
        Safety::feedWatchdog();

        // 1. Leer sensores (alta frecuencia)
        SensorsI2C::update();
        EncoderIR::updateRPM();

        // 2. Verificar limit switches
        LimitSwitches::checkAndPublishChanges();

        // 2.5 Auto-calibración de posición por limit switches
        //     Esta lógica es independiente del modo y corrige deriva de sensores.
        {
            bool lsStartNow = LimitSwitches::isStartActive();
            bool lsEndNow   = LimitSwitches::isEndActive();

            // Flanco de activación LS_START → Calibrar a 0 cm
            if (lsStartNow && !_prevLsStart) {
                EncoderIR::setPositionCm(0.0f);
                SensorsI2C::setMagPositionCm(0.0f);
                Serial.println("[main] Auto-calibración: LS_START → posición = 0 cm");
            }

            // Flanco de activación LS_END → Calibrar a TRACK_LENGTH_CM
            if (lsEndNow && !_prevLsEnd) {
                EncoderIR::setPositionCm(TRACK_LENGTH_CM);
                SensorsI2C::setMagPositionCm(TRACK_LENGTH_CM);
                Serial.println("[main] Auto-calibración: LS_END → posición = 90 cm");
            }

            _prevLsStart = lsStartNow;
            _prevLsEnd   = lsEndNow;
        }

        // 3. Safety check
        Safety::check();

        // 4. Procesar comandos MQTT pendientes (desde cola thread-safe)
        MqttComm::processCommandQueue();

        // 5. Ejecutar modo activo
        switch (activeMode) {
            case MODE_SWEEP_PWM:  RangoPwmMode::loop(); break;
            case MODE_OPEN_LOOP:  LazoAbiertoMode::loop(); break;
            case MODE_PID_POS:    PidPosicionMode::loop(); break;
            default: break;
        }

        // 6. Actualizar telemetría compartida (para Core 1)
        if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
            buildTelemetry(sharedTelemetry);
            xSemaphoreGive(telemetryMutex);
        }

        // Esperar hasta siguiente período (timing preciso)
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

// ══════════════════════════════════════════════════════════════════
// TAREA CORE 1: Comunicación (flexible, no crítico)
// ══════════════════════════════════════════════════════════════════
void taskCommunication(void* pvParameters) {
    Serial.println("[Core1] Tarea de comunicación iniciada");
    
    unsigned long lastTelemetryMs = 0;
    unsigned long lastStatusMs    = 0;
    unsigned long lastDisplayMs   = 0;

    for (;;) {
        unsigned long now = millis();

        // 1. MQTT keep-alive (reconexión automática)
        MqttComm::loop();

        // Mantener safety alive mientras MQTT conectado
        if (MqttComm::isConnected()) {
            Safety::markMqttActivity();
        }

        // 2. Publicar telemetría @ 5 Hz normal / 20 Hz durante PID activo
        {
            TelemetryPacket pkt;
            if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                pkt = sharedTelemetry;  // Copia local
                xSemaphoreGive(telemetryMutex);
            }
            uint32_t telemInterval = pkt.telemetry_fast
                ? TELEMETRY_FAST_INTERVAL_MS
                : TELEMETRY_INTERVAL_MS;
            if ((now - lastTelemetryMs) >= telemInterval) {
                lastTelemetryMs = now;
                if (pkt.telemetry_fast) {
                    MqttComm::publishTelemetryFast(pkt);  // JSON compacto ~150 bytes
                } else {
                    MqttComm::publishTelemetry(pkt);      // JSON completo ~500 bytes
                }
            }
        }

        // 3. Publicar estado @ 2 Hz (cada 500ms)
        if ((now - lastStatusMs) >= STATUS_INTERVAL_MS) {
            lastStatusMs = now;

            TelemetryPacket pkt;
            if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                pkt = sharedTelemetry;
                xSemaphoreGive(telemetryMutex);
            }

            MqttComm::publishStatus(
                getActiveModeStateStr(), activeMode,
                pkt.ls_start, pkt.ls_end,
                MqttComm::getWifiRssi(),
                SensorsI2C::isAngleCalibrated(),
                SensorsI2C::isMagPosCalibrated(),
                SensorsI2C::isMPUCalibrated());
        }

        // 4. Actualizar OLED @ 2 Hz (puede bloquear 92ms)
        //    Desactivar durante telemetría rápida para no perder ventanas de 50ms
        if ((now - lastDisplayMs) >= 500) {
            lastDisplayMs = now;  // Siempre actualizar timestamp para evitar re-entrada cada 10ms
            TelemetryPacket pktOled;
            bool isFast = false;
            if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                pktOled = sharedTelemetry;
                isFast = pktOled.telemetry_fast;
                xSemaphoreGive(telemetryMutex);
            }
            if (!isFast) {
                DisplayOLED::update(pktOled, getActiveModeStateStr());
            }
        }

        // Yield para no monopolizar CPU
        vTaskDelay(pdMS_TO_TICKS(COMM_TASK_PERIOD_MS));
    }
}

// ══════════════════════════════════════════════════
// Setup
// ══════════════════════════════════════════════════
void setup() {
    // Desactivar brownout detector
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(500);
    Serial.println("\n[main] ═══ SPISC — Firmware Dual-Core ═══");

    // 1. Motor
    Motor::begin();
    Serial.println("[main] Motor OK");

    // 2. Encoder IR
    EncoderIR::begin();
    Serial.println("[main] Encoder IR OK");

    // 3. Limit switches
    LimitSwitches::begin();
    Serial.println("[main] Limit switches OK");

    // 4. Mutex I2C para acceso thread-safe (antes de sensores)
    SensorsI2C::initI2CMutex();

    // 5. Sensores I2C (AS5600x2 + MPU6050)
    SensorsI2C::begin();
    Serial.println("[main] Sensores I2C OK");

    // 6. OLED
    if (DisplayOLED::begin()) {
        DisplayOLED::showMessage("SPISC Dual-Core", "Iniciando...");
    }

    // 7. Crear mutex y cola para comunicación inter-core
    initTelemetryMutex();
    MqttComm::initCommandQueue();
    Serial.println("[main] Mutex y cola OK");

    // 8. WiFi + MQTT (con callback que encola comandos)
    MqttComm::onCommand(onMqttCommand);
    if (MqttComm::begin()) {
        Serial.println("[main] WiFi + MQTT OK");
        DisplayOLED::showMessage("SPISC Dual-Core", "MQTT conectado!");
    } else {
        Serial.println("[main] WiFi no conectó (reintentará)");
        DisplayOLED::showMessage("SPISC Dual-Core", "WiFi pendiente...");
    }

    // 10. Seguridad (timeout aumentado para tolerar reconexiones WiFi)
    Safety::setMqttTimeout(10000);
    Serial.println("[main] Safety OK");

    // 9. Modo inicial
    activeMode = MODE_IDLE;

    // ═══════════════════════════════════════════════════════════════
    // CREAR TAREAS EN CORES SEPARADOS
    // ═══════════════════════════════════════════════════════════════

    // Tarea de sensores en CORE 0 (prioridad alta)
    xTaskCreatePinnedToCore(
        taskSensorsAndControl,      // Función
        "Sensors",                  // Nombre
        SENSORS_TASK_STACK,         // Stack (bytes)
        NULL,                       // Parámetros
        SENSORS_TASK_PRIORITY,      // Prioridad (alta)
        &taskSensorsHandle,         // Handle
        0                           // Core 0
    );
    Serial.println("[main] Tarea Sensors creada en Core 0");

    // Tarea de comunicación en CORE 1 (prioridad media)
    xTaskCreatePinnedToCore(
        taskCommunication,          // Función
        "Comm",                     // Nombre
        COMM_TASK_STACK,            // Stack mayor para JSON + WiFi
        NULL,                       // Parámetros
        COMM_TASK_PRIORITY,         // Prioridad media
        &taskCommHandle,            // Handle
        1                           // Core 1
    );
    Serial.println("[main] Tarea Comm creada en Core 1");

    Serial.println("[main] ═══ Sistema dual-core activo ═══");
    Serial.println("[main] Esperando comandos MQTT...\n");
}

// ══════════════════════════════════════════════════
// Loop — Vacío (todo corre en tareas FreeRTOS)
// ══════════════════════════════════════════════════
void loop() {
    // Todo el trabajo se hace en las tareas FreeRTOS
    // Este loop se ejecuta en Core 1 con prioridad baja
    vTaskDelay(portMAX_DELAY);
}
