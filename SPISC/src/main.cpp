/*
 * ═══════════════════════════════════════════════════════════════
 *  main.cpp — Orquestador modular del firmware SPISC.
 *
 *  Inicializa todos los módulos de hardware compartidos, lee
 *  sensores, aplica seguridad y enruta al modo activo.
 *
 *  Modos soportados: SWEEP_PWM, OPEN_LOOP (extensible).
 * ═══════════════════════════════════════════════════════════════
 */

#include <Arduino.h>

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

// ══════════════════════════════════════════════════
// Modo activo
// ══════════════════════════════════════════════════
static FirmwareMode activeMode = MODE_IDLE;

// ══════════════════════════════════════════════════
// Intervalos de publicación
// ══════════════════════════════════════════════════
static unsigned long lastTelemetryMs = 0;
static unsigned long lastStatusMs    = 0;
static unsigned long lastDisplayMs   = 0;

// ══════════════════════════════════════════════════
// Construir paquete de telemetría
// ══════════════════════════════════════════════════
static void buildTelemetry(TelemetryPacket& pkt) {
    // Encoder IR
    pkt.pulsos_ir = EncoderIR::getPulseCount();
    pkt.vueltas_ir = EncoderIR::getVueltas();
    pkt.rps_ir    = EncoderIR::getRPS();
    pkt.rpm_ir    = EncoderIR::getRPM();
    pkt.pos_ir_cm = EncoderIR::getPosCm();

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
    pkt.ts   = millis();
    pkt.modo = activeMode;
}

// ══════════════════════════════════════════════════
// Obtener string del estado del modo activo
// ══════════════════════════════════════════════════
static const char* getActiveModeStateStr() {
    switch (activeMode) {
        case MODE_SWEEP_PWM:  return RangoPwmMode::getStateStr();
        case MODE_OPEN_LOOP:  return LazoAbiertoMode::getStateStr();
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
        default: break;
    }
}

// ══════════════════════════════════════════════════
// Callback MQTT global
// ══════════════════════════════════════════════════
static void onMqttCommand(const char* topic, const char* payload) {
    Safety::markMqttActivity();

    // Parada de emergencia global
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        Motor::freno();
        // Pasar también al modo activo
        switch (activeMode) {
            case MODE_SWEEP_PWM:  RangoPwmMode::onCommand(topic, payload); break;
            case MODE_OPEN_LOOP:  LazoAbiertoMode::onCommand(topic, payload); break;
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
            else if (strstr(modeKey, "\"IDLE\""))      m = MODE_IDLE;
            switchMode(m);
        }
        return;
    }

    // Calibración
    if (strcmp(topic, TOPIC_CMD_CALIBRATE) == 0) {
        SensorsI2C::calibrateAngle();
        return;
    }

    // Enrutar al modo activo
    switch (activeMode) {
        case MODE_SWEEP_PWM:  RangoPwmMode::onCommand(topic, payload); break;
        case MODE_OPEN_LOOP:  LazoAbiertoMode::onCommand(topic, payload); break;
        default: break;
    }
}

// ══════════════════════════════════════════════════
// Setup
// ══════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[main] ═══ SPISC — Firmware Modular ═══");

    // 1. Motor
    Motor::begin();
    Serial.println("[main] Motor OK");

    // 2. Encoder IR
    EncoderIR::begin();
    Serial.println("[main] Encoder IR OK");

    // 3. Limit switches
    LimitSwitches::begin();
    Serial.println("[main] Limit switches OK");

    // 4. Sensores I2C (AS5600x2 + MPU6050)
    SensorsI2C::begin();
    Serial.println("[main] Sensores I2C OK");

    // 5. OLED
    if (DisplayOLED::begin()) {
        DisplayOLED::showMessage("SPISC Pendulum", "Conectando WiFi...");
    }

    // 6. WiFi + MQTT
    MqttComm::onCommand(onMqttCommand);
    if (MqttComm::begin()) {
        Serial.println("[main] WiFi + MQTT OK");
        DisplayOLED::showMessage("SPISC Pendulum", "MQTT conectado!");
    } else {
        Serial.println("[main] WiFi no conectó (reintentará)");
        DisplayOLED::showMessage("SPISC Pendulum", "WiFi pendiente...");
    }

    // 7. Seguridad
    Safety::setMqttTimeout(0);  // Deshabilitado por defecto
    Serial.println("[main] Safety OK");

    // Modo inicial: IDLE
    activeMode = MODE_IDLE;
    Serial.println("[main] Listo. Esperando comandos MQTT...\n");
}

// ══════════════════════════════════════════════════
// Loop
// ══════════════════════════════════════════════════
void loop() {
    unsigned long now = millis();

    // 1. MQTT keep-alive
    MqttComm::loop();

    // 2. Leer sensores (alta frecuencia)
    SensorsI2C::update();
    EncoderIR::updateRPM();

    // 3. Publicar cambios de limit switches
    LimitSwitches::checkAndPublishChanges();

    // 4. Safety
    Safety::check();

    // 5. Ejecutar modo activo
    switch (activeMode) {
        case MODE_SWEEP_PWM:  RangoPwmMode::loop(); break;
        case MODE_OPEN_LOOP:  LazoAbiertoMode::loop(); break;
        default: break;  // IDLE: nada
    }

    // 6. Publicar telemetría periódica
    if ((now - lastTelemetryMs) >= TELEMETRY_INTERVAL) {
        lastTelemetryMs = now;
        TelemetryPacket pkt = {};
        buildTelemetry(pkt);
        MqttComm::publishTelemetry(pkt);
    }

    // 7. Publicar estado periódico
    if ((now - lastStatusMs) >= STATUS_INTERVAL) {
        lastStatusMs = now;
        MqttComm::publishStatus(
            getActiveModeStateStr(), activeMode,
            LimitSwitches::isStartActive(),
            LimitSwitches::isEndActive(),
            MqttComm::getWifiRssi());
    }

    // 8. Actualizar OLED (~5 Hz)
    if ((now - lastDisplayMs) >= 200) {
        lastDisplayMs = now;
        TelemetryPacket pkt = {};
        buildTelemetry(pkt);
        DisplayOLED::update(pkt, getActiveModeStateStr());
    }
}
