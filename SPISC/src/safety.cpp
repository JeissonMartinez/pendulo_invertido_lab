/**
 * safety.cpp — Implementación del módulo de seguridad.
 */
#include "safety.h"
#include "motor_control.h"
#include "limit_switches.h"

namespace Safety {

static unsigned long _lastMqttActivity = 0;
static unsigned long _mqttTimeout      = 0;  // 0 = deshabilitado
static unsigned long _lastTimeoutLog   = 0;  // throttle de log

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

}  // namespace Safety
