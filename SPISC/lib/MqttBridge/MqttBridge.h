/**
 * MqttBridge — Librería para conexión WiFi + MQTT del ESP32.
 * Maneja reconexión automática y publicación JSON.
 */

#ifndef MQTT_BRIDGE_H
#define MQTT_BRIDGE_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "wifi_mqtt_config.h"

// Callback para mensajes MQTT recibidos
typedef void (*MqttMessageCallback)(const char* topic, const char* payload);

class MqttBridge {
public:
    MqttBridge();

    /**
     * Inicializa WiFi y MQTT. Bloquea hasta que WiFi conecte (con timeout).
     * @return true si WiFi conectó exitosamente.
     */
    bool begin();

    /**
     * Llamar en cada iteración de loop().
     * Mantiene la conexión WiFi y MQTT activa, procesa mensajes.
     */
    void loop();

    /** Publica un string en un tópico MQTT. */
    bool publish(const char* topic, const char* payload);

    /** Suscribirse a un tópico. */
    bool subscribe(const char* topic);

    /** Registrar callback para mensajes recibidos. */
    void onMessage(MqttMessageCallback cb);

    /** ¿Está conectado a WiFi? */
    bool isWifiConnected() const;

    /** ¿Está conectado al broker MQTT? */
    bool isMqttConnected();

    /** RSSI de la señal WiFi. */
    int getWifiRssi() const;

private:
    WiFiClient   _wifiClient;
    PubSubClient _mqttClient;
    MqttMessageCallback _callback;

    unsigned long _lastReconnectAttempt;
    unsigned long _reconnectInterval;

    static constexpr uint8_t MAX_SUBS = 10;
    const char* _subTopics[MAX_SUBS];
    uint8_t     _subCount;

    bool _connectWifi();
    bool _connectWifiAP();   // Modo Access Point
    bool _connectWifiSTA();  // Modo Station
    bool _connectMqtt();
    void _resubscribe();

    static MqttBridge* _instance;
    static void _staticCallback(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_BRIDGE_H
