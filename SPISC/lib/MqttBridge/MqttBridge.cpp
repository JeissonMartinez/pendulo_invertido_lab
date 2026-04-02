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
    , _reconnectInterval(500)  // Inicio más rápido (500ms vs 1s)
    , _subCount(0)
{
    _instance = this;
}

bool MqttBridge::begin() {
    Serial.println("[MqttBridge] Iniciando...");

    if (!_connectWifi()) {
        return false;
    }

    // Configurar timeout del socket para detectar desconexiones más rápido
    _wifiClient.setTimeout(5);  // 5 segundos timeout TCP
    
    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    _mqttClient.setCallback(_staticCallback);
    _mqttClient.setBufferSize(1024);
    _mqttClient.setKeepAlive(5);  // Keepalive MQTT cada 5 segundos (default: 15)

    _connectMqtt();
    return true;
}

void MqttBridge::loop() {
#ifdef CFG_WIFI_AP
    // ── Modo AP: verificar si hay clientes conectados ──
    static unsigned long lastClientCheck = 0;
    static uint8_t lastClientCount = 0;
    
    uint8_t clientCount = WiFi.softAPgetStationNum();
    
    // Log cambios en clientes conectados
    if (clientCount != lastClientCount) {
        Serial.printf("[MqttBridge] Clientes AP: %d\n", clientCount);
        lastClientCount = clientCount;
    }
    
    // Si no hay clientes, no tiene sentido intentar MQTT
    if (clientCount == 0) {
        // Mostrar recordatorio cada 10 segundos
        if (millis() - lastClientCheck >= 10000) {
            lastClientCheck = millis();
            Serial.printf("[MqttBridge] Esperando cliente... Conectar a WiFi '%s'\n", 
                          WIFI_AP_SSID);
        }
        return;
    }
    
#else
    // ── Modo STA: verificar conexión al router ──
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MqttBridge] WiFi desconectado, reconectando...");
        _connectWifiSTA();
        return;
    }
#endif

    // ── Verificar/reconectar MQTT (común para ambos modos) ──
    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= _reconnectInterval) {
            _lastReconnectAttempt = now;
            
            Serial.printf("[MqttBridge] Intentando MQTT %s:%d...\n", 
                          MQTT_BROKER, MQTT_PORT);
            
            if (_connectMqtt()) {
                _reconnectInterval = 500;  // Reset backoff
                Serial.println("[MqttBridge] MQTT conectado!");
            } else {
                // Exponential backoff: 500ms → 1s → 2s → ... → 5s max
                _reconnectInterval = min(_reconnectInterval * 2, 5000UL);
                Serial.printf("[MqttBridge] MQTT fallo. Reintento en %lu ms\n", 
                              _reconnectInterval);
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
#ifdef CFG_WIFI_AP
    // En modo AP, RSSI no aplica (somos el AP, no un cliente)
    // Retornar indicador basado en clientes conectados
    return WiFi.softAPgetStationNum() > 0 ? -30 : -100;
#else
    return WiFi.RSSI();
#endif
}

// ── Selector de modo WiFi ────────────────────────
bool MqttBridge::_connectWifi() {
#ifdef CFG_WIFI_AP
    return _connectWifiAP();
#else
    return _connectWifiSTA();
#endif
}

// ── Modo Access Point ────────────────────────────
bool MqttBridge::_connectWifiAP() {
    Serial.println("[MqttBridge] Iniciando modo Access Point...");
    
    // Desconectar cualquier conexión previa
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_MODE_AP);  // Usar enum del framework ESP32
    
    // Configurar IP estática
    if (!WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET)) {
        Serial.println("[MqttBridge] Error configurando IP del AP");
        return false;
    }
    
    // Iniciar AP
    bool success = WiFi.softAP(
        WIFI_AP_SSID, 
        WIFI_AP_PASSWORD,
        WIFI_AP_CHANNEL,
        false,              // SSID visible (no oculto)
        WIFI_AP_MAX_CONN    // Máximo clientes
    );
    
    if (!success) {
        Serial.println("[MqttBridge] Error al crear Access Point");
        return false;
    }
    
    Serial.println("[MqttBridge] ═══════════════════════════════════");
    Serial.println("[MqttBridge] ACCESS POINT ACTIVO");
    Serial.printf( "[MqttBridge]   SSID: %s\n", WIFI_AP_SSID);
    Serial.printf( "[MqttBridge]   Password: %s\n", WIFI_AP_PASSWORD);
    Serial.printf( "[MqttBridge]   IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf( "[MqttBridge]   Canal: %d\n", WIFI_AP_CHANNEL);
    Serial.println("[MqttBridge] ═══════════════════════════════════");
    Serial.printf( "[MqttBridge] Esperando conexión a MQTT en %s:%d\n", 
                   MQTT_BROKER, MQTT_PORT);
    
    return true;
}

// ── Modo Station (original, renombrado) ──────────
bool MqttBridge::_connectWifiSTA() {
#ifdef CFG_WIFI_STA
    Serial.printf("[MqttBridge] Conectando a WiFi '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_15dBm);
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
#else
    return false;  // STA no habilitado
#endif
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
    const unsigned int maxLen = 511;
    unsigned int safeLen = (length < maxLen) ? length : maxLen;
    char buf[512];
    memcpy(buf, payload, safeLen);
    buf[safeLen] = '\0';

    // Llamar al callback registrado (MqttComm se encarga de encolar)
    _instance->_callback(topic, buf);
}
