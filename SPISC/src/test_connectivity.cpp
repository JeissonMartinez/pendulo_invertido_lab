/**
 * Test de conectividad WiFi + MQTT — Péndulo Invertido.
 *
 * Publica datos dummy en los tópicos estándar para verificar
 * que la cadena completa ESP32 → Mosquitto → FastAPI → Vue funciona.
 *
 * Compilar:  pio run -e test_connectivity -t upload
 * Monitor:   pio device monitor -e test_connectivity
 */

#include <Arduino.h>
#include "MqttBridge.h"

MqttBridge mqtt;

unsigned long lastTelemetry = 0;
unsigned long lastStatus    = 0;

// Callback para mensajes recibidos
void onMqttMessage(const char* topic, const char* payload) {
    Serial.printf("[RX] %s → %s\n", topic, payload);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("==============================================");
    Serial.println("  TEST CONECTIVIDAD WiFi + MQTT");
    Serial.println("==============================================");

    if (!mqtt.begin()) {
        Serial.println("[ERROR] No se pudo conectar a WiFi. Reiniciando en 5s...");
        delay(5000);
        ESP.restart();
    }

    // Suscribirse a tópicos de comandos
    mqtt.subscribe(TOPIC_CMD_MOTOR);
    mqtt.subscribe(TOPIC_CMD_MODE);
    mqtt.subscribe(TOPIC_CMD_STOP);
    mqtt.subscribe(TOPIC_CMD_SWEEP);
    mqtt.onMessage(onMqttMessage);

    Serial.println("[OK] Publicando datos de prueba...");
}

void loop() {
    mqtt.loop();

    unsigned long now = millis();

    // Telemetría dummy a ~2 Hz
    if (now - lastTelemetry >= 500) {
        lastTelemetry = now;

        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"angle\":0,\"position\":45.0,\"cart_accel\":0,"
            "\"rpm\":0,\"pwm\":0,\"direction\":\"DETENIDO\",\"ts\":%lu}",
            now);
        mqtt.publish(TOPIC_TELEMETRY, buf);
        Serial.printf("[TX] telemetry: %s\n", buf);
    }

    // Estado a ~0.5 Hz
    if (now - lastStatus >= 2000) {
        lastStatus = now;

        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"state\":\"IDLE\",\"mode\":\"MANUAL\","
            "\"limits\":{\"start_switch\":false,\"end_switch\":false},"
            "\"errors\":[],\"wifi_rssi\":%d,"
            "\"calibrated_angle\":false,\"calibrated_position\":false}",
            mqtt.getWifiRssi());
        mqtt.publish(TOPIC_STATUS, buf);
        Serial.printf("[TX] status: %s\n", buf);
    }
}
