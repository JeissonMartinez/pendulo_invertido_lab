/**
 * MqttBridge — Implementación.
 */

#include "MqttBridge.h"

// Singleton para el callback estático de PubSubClient
MqttBridge* MqttBridge::_instance = nullptr;

MqttBridge::MqttBridge()
    : _mqttClient(_wifiClient)
    , _callback(nullptr)
    , _lastReconnectAttempt(0)
    , _reconnectInterval(1000)
    , _subCount(0)
{
    _instance = this;
}

bool MqttBridge::begin() {
    Serial.println("[MqttBridge] Iniciando...");

    if (!_connectWifi()) {
        return false;
    }

    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    _mqttClient.setCallback(_staticCallback);
    _mqttClient.setBufferSize(1024);

    _connectMqtt();
    return true;
}

void MqttBridge::loop() {
    // Verificar WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MqttBridge] WiFi desconectado, reconectando...");
        _connectWifi();
        return;
    }

    // Verificar MQTT
    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= _reconnectInterval) {
            _lastReconnectAttempt = now;
            if (_connectMqtt()) {
                _reconnectInterval = 1000; // reset backoff
            } else {
                // Exponential backoff: 1s → 2s → 4s → ... → 30s
                _reconnectInterval = min(_reconnectInterval * 2, 30000UL);
            }
        }
    }

    _mqttClient.loop();
}

bool MqttBridge::publish(const char* topic, const char* payload) {
    if (!_mqttClient.connected()) return false;
    return _mqttClient.publish(topic, payload);
}

bool MqttBridge::subscribe(const char* topic) {
    // Store topic for re-subscribe on reconnect
    if (_subCount < MAX_SUBS) {
        _subTopics[_subCount++] = topic;
    }
    if (!_mqttClient.connected()) return false;
    return _mqttClient.subscribe(topic);
}

void MqttBridge::onMessage(MqttMessageCallback cb) {
    _callback = cb;
}

bool MqttBridge::isWifiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool MqttBridge::isMqttConnected() {
    return _mqttClient.connected();
}

int MqttBridge::getWifiRssi() const {
    return WiFi.RSSI();
}

// ── Conexión WiFi ────────────────────────────────
bool MqttBridge::_connectWifi() {
    Serial.printf("[MqttBridge] Conectando a WiFi '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                  // Desactivar modem-sleep para mantener conexión estable
    WiFi.setTxPower(WIFI_POWER_15dBm);     // TX 15 dBm (brownout detector desactivado)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            Serial.println("[MqttBridge] Timeout WiFi (15s)");
            return false;
        }
        delay(250);
        Serial.print(".");
    }

    Serial.printf("\n[MqttBridge] WiFi OK — IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
}

// ── Conexión MQTT ────────────────────────────────
bool MqttBridge::_connectMqtt() {
    Serial.printf("[MqttBridge] Conectando a MQTT %s:%d ...\n", MQTT_BROKER, MQTT_PORT);

    if (_mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.println("[MqttBridge] MQTT conectado");
        _resubscribe();
        return true;
    }

    Serial.printf("[MqttBridge] MQTT fallo, rc=%d\n", _mqttClient.state());
    return false;
}

void MqttBridge::_resubscribe() {
    for (uint8_t i = 0; i < _subCount; i++) {
        _mqttClient.subscribe(_subTopics[i]);
    }
}

// ── Callback estático → instancia ────────────────
void MqttBridge::_staticCallback(char* topic, byte* payload, unsigned int length) {
    if (!_instance || !_instance->_callback) return;

    // Crear string null-terminated del payload
    // Limit to buffer size to prevent overflow
    const unsigned int maxLen = 1023;
    unsigned int safeLen = (length < maxLen) ? length : maxLen;
    char buf[1024];
    memcpy(buf, payload, safeLen);
    buf[safeLen] = '\0';

    _instance->_callback(topic, buf);
}
