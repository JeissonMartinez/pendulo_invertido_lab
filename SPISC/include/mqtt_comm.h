/**
 * mqtt_comm.h — Comunicación MQTT unificada.
 * Wrapper sobre MqttBridge + publicación de telemetría/estado.
 */
#ifndef MQTT_COMM_H
#define MQTT_COMM_H

#include <Arduino.h>
#include <MqttBridge.h>
#include "telemetry_data.h"

namespace MqttComm {

/** Tipo de callback para comandos recibidos. */
typedef void (*CommandCallback)(const char* topic, const char* payload);

/** Inicializa WiFi + MQTT y suscribe a todos los tópicos de comando. */
bool begin();

/** Llamar en cada loop() para mantener conexión. */
void loop();

/** Registra callback para procesar comandos entrantes. */
void onCommand(CommandCallback cb);

/** Publica un paquete de telemetría completo. */
void publishTelemetry(const TelemetryPacket& pkt);

/** Publica estado del sistema. */
void publishStatus(const char* state, FirmwareMode mode,
                   bool lsStart, bool lsEnd, int wifiRssi,
                   bool calAngle, bool calPosition, bool calMPU);

/** Publica cambio de limit switches. */
void publishLimits(bool lsStart, bool lsEnd);

/** Publica datos genéricos en un tópico. */
bool publish(const char* topic, const char* payload);

/** RSSI de la señal WiFi. */
int getWifiRssi();

/** ¿Está conectado a MQTT? */
bool isConnected();

}  // namespace MqttComm

#endif // MQTT_COMM_H
