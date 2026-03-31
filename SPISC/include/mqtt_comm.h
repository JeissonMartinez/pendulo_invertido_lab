/**
 * mqtt_comm.h — Comunicación MQTT unificada.
 * Wrapper sobre MqttBridge + publicación de telemetría/estado.
 * 
 * Arquitectura Dual-Core:
 * - Los callbacks MQTT se encolan para procesamiento en Core 0
 * - La publicación ocurre desde Core 1 (no bloquea sensores)
 */
#ifndef MQTT_COMM_H
#define MQTT_COMM_H

#include <Arduino.h>
#include <MqttBridge.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "telemetry_data.h"

// ══════════════════════════════════════════════════
// Estructura para comandos en cola (Core 1 → Core 0)
// ══════════════════════════════════════════════════
struct MqttCommand {
    char topic[64];
    char payload[512];
};

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

// ══════════════════════════════════════════════════
// Cola de comandos thread-safe (Dual-Core)
// ══════════════════════════════════════════════════

/** Inicializar cola de comandos. Llamar antes de crear tareas. */
void initCommandQueue();

/** 
 * Encolar comando para procesamiento en Core 0.
 * Llamado desde el callback MQTT en Core 1.
 * @return true si se encoló correctamente.
 */
bool enqueueCommand(const char* topic, const char* payload);

/**
 * Procesar comandos pendientes en la cola.
 * Llamar desde la tarea de sensores en Core 0.
 */
void processCommandQueue();

}  // namespace MqttComm

#endif // MQTT_COMM_H
