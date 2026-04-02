/**
 * mqtt_comm.cpp — Implementación de comunicación MQTT unificada.
 * 
 * Arquitectura Dual-Core:
 * - La cola de comandos permite procesar MQTT en Core 0 sin bloqueos
 * - Los comandos se encolan desde el callback (Core 1) y procesan en taskSensorsAndControl (Core 0)
 */
#include "mqtt_comm.h"
#include "hardware_pins.h"
#include "time_sync.h"
#include <wifi_mqtt_config.h>
#include <cstring>
#include <cstdlib>

namespace MqttComm {

static MqttBridge _bridge;
static char _jsonBuf[768];   // Buffer para JSON (ampliado para telemetría extendida)

// ══════════════════════════════════════════════════
// Cola de comandos thread-safe
// ══════════════════════════════════════════════════
static QueueHandle_t _commandQueue = NULL;
static CommandCallback _commandCallback = nullptr;

// ══════════════════════════════════════════════════
// Callback interno que encola comandos (Dual-Core)
// ══════════════════════════════════════════════════
static void _internalCallback(const char* topic, const char* payload) {
    // Procesar sincronización de tiempo directamente (no encolar)
    if (strcmp(topic, TOPIC_TIME_SYNC) == 0) {
        // Parseo simple: {"ts": 1234567890123}
        const char* tsStart = strstr(payload, "\"ts\":");
        if (tsStart) {
            tsStart += 5;  // Saltar "ts":
            while (*tsStart == ' ') tsStart++;  // Saltar espacios
            uint64_t serverTimeMs = strtoull(tsStart, nullptr, 10);
            if (serverTimeMs > 0) {
                TimeSync::setServerTime(serverTimeMs);
            }
        }
        return;
    }
    // Encolar comando para procesamiento thread-safe en Core 0
    enqueueCommand(topic, payload);
}

bool begin() {
    bool ok = _bridge.begin();
    if (ok) {
        // Suscribirse a todos los tópicos de comando
        _bridge.subscribe(TOPIC_CMD_MOTOR);
        _bridge.subscribe(TOPIC_CMD_MODE);
        _bridge.subscribe(TOPIC_CMD_CALIBRATE);
        _bridge.subscribe(TOPIC_CMD_CONFIG);
        _bridge.subscribe(TOPIC_CMD_STOP);
        _bridge.subscribe(TOPIC_CMD_SWEEP);
        _bridge.subscribe(TOPIC_CMD_OPENLOOP);
        _bridge.subscribe(TOPIC_CMD_PID_POS);
        _bridge.subscribe(TOPIC_CMD_DEBUG);
        _bridge.subscribe(TOPIC_TIME_SYNC);  // Sincronización de tiempo
        
        // Registrar callback interno que encola comandos
        _bridge.onMessage(_internalCallback);
    }
    return ok;
}

void loop() {
    _bridge.loop();
}

void onCommand(CommandCallback cb) {
    _commandCallback = cb;  // Guardar callback para processCommandQueue
    // Ya no registramos con _bridge.onMessage() aquí
    // El callback interno ya está registrado en begin()
}

void publishTelemetry(const TelemetryPacket& p) {
    snprintf(_jsonBuf, sizeof(_jsonBuf),
        "{"
        "\"pulsos_ir\":%lu,\"vueltas_ir\":%.2f,\"rps_ir\":%.2f,\"rpm_ir\":%.1f,\"pos_ir_cm\":%.2f,"
        "\"rps_mag\":%.2f,\"rpm_mag\":%.1f,\"pos_mag_cm\":%.2f,"
        "\"angle_pend\":%.2f,\"vel_ang_pend\":%.1f,"
        "\"mpu_ax\":%.3f,\"mpu_ay\":%.3f,\"mpu_az\":%.3f,"
        "\"mpu_gx\":%.2f,\"mpu_gy\":%.2f,\"mpu_gz\":%.2f,"
        "\"mpu_pitch\":%.2f,\"mpu_roll\":%.2f,"
        "\"pwm\":%d,\"direction\":%d,"
        "\"ls_start\":%s,\"ls_end\":%s,"
        "\"ts\":%lu,\"modo\":\"%s\",\"ol_state\":\"%s\","
        // Control PID Posición
        "\"pid_error\":%.3f,\"pid_integral\":%.3f,\"pid_derivada\":%.3f,"
        "\"pid_output\":%.1f,\"pid_output_sat\":%.1f,"
        "\"pid_setpoint\":%.2f,\"pid_kp\":%.2f,\"pid_ki\":%.3f,\"pid_kd\":%.3f,"
        "\"pid_zona_seg\":%.1f,\"pid_tolerancia\":%.2f,"
        "\"pid_sensor_type\":%d,\"pid_pwm_min\":%d,\"pid_pwm_max\":%d,"
        // Mantener campos de compatibilidad con frontend actual
        "\"angle\":%.2f,\"position\":%.2f,\"cart_accel\":%.3f,\"rpm\":%.1f,"
        "\"posicion\":%.2f,\"direccion\":%d"
        "}",
        (unsigned long)p.pulsos_ir, p.vueltas_ir, p.rps_ir, p.rpm_ir, p.pos_ir_cm,
        p.rps_mag, p.rpm_mag, p.pos_mag_cm,
        p.angle_pend, p.vel_ang_pend,
        p.mpu_ax, p.mpu_ay, p.mpu_az,
        p.mpu_gx, p.mpu_gy, p.mpu_gz,
        p.mpu_pitch, p.mpu_roll,
        p.pwm, (int)p.direction,
        p.ls_start ? "true" : "false", p.ls_end ? "true" : "false",
        p.ts, firmwareModeToStr(p.modo), p.ol_state ? p.ol_state : "",
        // PID
        p.pid_error, p.pid_integral, p.pid_derivada,
        p.pid_output, p.pid_output_sat,
        p.pid_setpoint, p.pid_kp, p.pid_ki, p.pid_kd,
        p.pid_zona_seg, p.pid_tolerancia,
        p.pid_sensor_type, p.pid_pwm_min, p.pid_pwm_max,
        // Compatibilidad: angle=angle_pend, position=pos_mag_cm, cart_accel=mpu_ax, rpm=rpm_ir
        p.angle_pend, p.pos_mag_cm, p.mpu_ax, p.rpm_ir,
        p.pos_mag_cm, (int)p.direction
    );
    _bridge.publish(TOPIC_TELEMETRY, _jsonBuf);
}

void publishStatus(const char* state, FirmwareMode mode,
                   bool lsStart, bool lsEnd, int wifiRssi,
                   bool calAngle, bool calPosition, bool calMPU) {
    snprintf(_jsonBuf, sizeof(_jsonBuf),
        "{\"estado\":\"%s\",\"modo\":\"%s\","
        "\"ls_inicio\":%s,\"ls_fin\":%s,"
        "\"wifi_rssi\":%d,"
        "\"calibrated_angle\":%s,\"calibrated_position\":%s,\"calibrated_mpu\":%s}",
        state, firmwareModeToStr(mode),
        lsStart ? "true" : "false",
        lsEnd   ? "true" : "false",
        wifiRssi,
        calAngle    ? "true" : "false",
        calPosition ? "true" : "false",
        calMPU      ? "true" : "false");
    _bridge.publish(TOPIC_STATUS, _jsonBuf);
}

void publishLimits(bool lsStart, bool lsEnd) {
    snprintf(_jsonBuf, sizeof(_jsonBuf),
        "{\"inicio\":%s,\"fin\":%s}",
        lsStart ? "true" : "false",
        lsEnd   ? "true" : "false");
    _bridge.publish(TOPIC_LIMITS, _jsonBuf);
}

bool publish(const char* topic, const char* payload) {
    return _bridge.publish(topic, payload);
}

int getWifiRssi()  { return _bridge.getWifiRssi(); }
bool isConnected() { return _bridge.isMqttConnected(); }

// ══════════════════════════════════════════════════
// Implementación de cola de comandos (Dual-Core)
// ══════════════════════════════════════════════════

void initCommandQueue() {
    if (_commandQueue == NULL) {
        _commandQueue = xQueueCreate(10, sizeof(MqttCommand));
        if (_commandQueue == NULL) {
            Serial.println("[MqttComm] Error creando cola de comandos!");
        } else {
            Serial.println("[MqttComm] Cola de comandos inicializada");
        }
    }
}

bool enqueueCommand(const char* topic, const char* payload) {
    if (_commandQueue == NULL) {
        Serial.println("[MqttComm] Cola no inicializada, procesando directo");
        // Fallback: procesar directamente (modo single-core)
        if (_commandCallback) {
            _commandCallback(topic, payload);
        }
        return true;
    }
    
    MqttCommand cmd;
    strncpy(cmd.topic, topic, sizeof(cmd.topic) - 1);
    cmd.topic[sizeof(cmd.topic) - 1] = '\0';
    strncpy(cmd.payload, payload, sizeof(cmd.payload) - 1);
    cmd.payload[sizeof(cmd.payload) - 1] = '\0';
    
    // Intentar encolar sin bloquear (xQueueSend con timeout 0)
    if (xQueueSend(_commandQueue, &cmd, 0) != pdTRUE) {
        Serial.println("[MqttComm] Cola de comandos llena!");
        return false;
    }
    
    return true;
}

void processCommandQueue() {
    if (_commandQueue == NULL || _commandCallback == NULL) {
        return;
    }
    
    MqttCommand cmd;
    // Procesar todos los comandos pendientes sin bloquear
    while (xQueueReceive(_commandQueue, &cmd, 0) == pdTRUE) {
        _commandCallback(cmd.topic, cmd.payload);
    }
}

}  // namespace MqttComm
