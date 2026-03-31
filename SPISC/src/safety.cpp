/**
 * safety.cpp — Implementación del módulo de seguridad.
 * 
 * Arquitectura Dual-Core:
 * - Watchdog hardware para reset automático si Core 0 se bloquea
 */
#include "safety.h"
#include "motor_control.h"
#include "limit_switches.h"
#include "esp_task_wdt.h"

namespace Safety {

static unsigned long _lastMqttActivity = 0;
static unsigned long _mqttTimeout      = 0;  // 0 = deshabilitado
static unsigned long _lastTimeoutLog   = 0;  // throttle de log
static bool _wdtInitialized = false;

void emergencyStop() {
    Motor::freno();
    Serial.println("[safety] EMERGENCY STOP");
}

bool check() {
    // 1. Limit switches: freno inmediato si se activan inesperadamente
    //    (cada modo debe manejar su propia lógica de LS;
    //     aquí solo limpiamos las flags ISR)
    LimitSwitches::wasStartTriggered();
    LimitSwitches::wasEndTriggered();

    // 2. Timeout MQTT (solo si está habilitado)
    if (_mqttTimeout > 0 && _lastMqttActivity > 0) {
        if ((millis() - _lastMqttActivity) > _mqttTimeout) {
            if ((millis() - _lastTimeoutLog) >= 5000) {
                Serial.println("[safety] Timeout MQTT — frenando motor");
                _lastTimeoutLog = millis();
            }
            Motor::freno();
            return false;
        }
    }

    return true;
}

void markMqttActivity() {
    _lastMqttActivity = millis();
}

void setMqttTimeout(unsigned long timeoutMs) {
    _mqttTimeout = timeoutMs;
}

// ══════════════════════════════════════════════════
// Watchdog Hardware (Dual-Core)
// ══════════════════════════════════════════════════

void initHardwareWatchdog(uint32_t timeoutSeconds) {
    if (_wdtInitialized) return;
    
    // La API depende de la versión de ESP-IDF
    // Arduino ESP32 usa la API antigua con dos parámetros
    esp_err_t err = esp_task_wdt_init(timeoutSeconds, true);  // timeout, panic_on_timeout
    
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE significa que ya está inicializado
        // Añadir la tarea actual al watchdog
        err = esp_task_wdt_add(NULL);
        if (err == ESP_OK || err == ESP_ERR_INVALID_ARG) {
            // ESP_ERR_INVALID_ARG puede significar que ya está añadida
            _wdtInitialized = true;
            Serial.printf("[safety] Watchdog hardware iniciado (%lu s)\n", timeoutSeconds);
        } else {
            Serial.printf("[safety] Error añadiendo tarea al watchdog: %d\n", err);
        }
    } else {
        Serial.printf("[safety] Error iniciando watchdog: %d\n", err);
    }
}

void feedWatchdog() {
    if (_wdtInitialized) {
        esp_task_wdt_reset();
    }
}

}  // namespace Safety
