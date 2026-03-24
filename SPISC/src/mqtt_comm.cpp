/**
 * mqtt_comm.cpp — Implementación de comunicación MQTT unificada.
 */
#include "mqtt_comm.h"
#include "hardware_pins.h"
#include <wifi_mqtt_config.h>

namespace MqttComm {

static MqttBridge _bridge;
static char _jsonBuf[768];   // Buffer para JSON (ampliado para telemetría extendida)

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
    }
    return ok;
}

void loop() {
    _bridge.loop();
}

void onCommand(CommandCallback cb) {
    _bridge.onMessage(cb);
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
        "\"ts\":%lu,\"modo\":\"%s\","
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
        p.ts, firmwareModeToStr(p.modo),
        // Compatibilidad: angle=angle_pend, position=pos_mag_cm, cart_accel=mpu_ax, rpm=rpm_ir
        p.angle_pend, p.pos_mag_cm, p.mpu_ax, p.rpm_ir,
        p.pos_mag_cm, (int)p.direction
    );
    _bridge.publish(TOPIC_TELEMETRY, _jsonBuf);
}

void publishStatus(const char* state, FirmwareMode mode,
                   bool lsStart, bool lsEnd, int wifiRssi) {
    snprintf(_jsonBuf, sizeof(_jsonBuf),
        "{\"estado\":\"%s\",\"modo\":\"%s\","
        "\"ls_inicio\":%s,\"ls_fin\":%s,"
        "\"wifi_rssi\":%d}",
        state, firmwareModeToStr(mode),
        lsStart ? "true" : "false",
        lsEnd   ? "true" : "false",
        wifiRssi);
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

}  // namespace MqttComm
