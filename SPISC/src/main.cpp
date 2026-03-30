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
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

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

// Forward declaration
static const char* getActiveModeStateStr();

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
        // Parsear tipo: {"type":"angle"}, {"type":"position"}, {"type":"mpu"}, {"type":"full"}
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
            // "angle" o sin tipo (compatibilidad)
            SensorsI2C::calibrateAngle();
        }
        return;
    }

    // Auto-switch de modo según el tópico del comando
    if (strcmp(topic, TOPIC_CMD_SWEEP) == 0 && activeMode != MODE_SWEEP_PWM) {
        switchMode(MODE_SWEEP_PWM);
    } else if (strcmp(topic, TOPIC_CMD_OPENLOOP) == 0 && activeMode != MODE_OPEN_LOOP) {
        switchMode(MODE_OPEN_LOOP);
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
    // Desactivar brownout detector (USB no entrega suficiente corriente para WiFi)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

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
    Safety::setMqttTimeout(2000);  // Freno si no hay MQTT en 2s
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

    // Mantener safety alive mientras MQTT está conectado
    if (MqttComm::isConnected()) {
        Safety::markMqttActivity();
    }

    // 2. Leer sensores (alta frecuencia)
    SensorsI2C::update();
    EncoderIR::updateRPM();
    yield();  // Dar tiempo al stack WiFi tras I2C

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
    if ((now - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryMs = now;
        TelemetryPacket pkt = {};
        buildTelemetry(pkt);
        MqttComm::publishTelemetry(pkt);
    }

    // 7. Publicar estado periódico
    if ((now - lastStatusMs) >= STATUS_INTERVAL_MS) {
        lastStatusMs = now;
        MqttComm::publishStatus(
            getActiveModeStateStr(), activeMode,
            LimitSwitches::isStartActive(),
            LimitSwitches::isEndActive(),
            MqttComm::getWifiRssi(),
            SensorsI2C::isAngleCalibrated(),
            SensorsI2C::isMagPosCalibrated(),
            SensorsI2C::isMPUCalibrated());
    }

    // 8. Actualizar OLED (~2 Hz para reducir bloqueo I2C)
    if ((now - lastDisplayMs) >= 500) {
        lastDisplayMs = now;
        TelemetryPacket pkt = {};
        buildTelemetry(pkt);
        DisplayOLED::update(pkt, getActiveModeStateStr());
    }
}
