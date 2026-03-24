/**
 * lazo_abierto_mode.cpp — Modo Lazo Abierto.
 *
 * Estados: OL_IDLE → OL_HOMING → OL_READY → OL_IMPULSE_RUNNING → OL_STOPPED
 *
 * Comandos (via pendulum/cmd/openloop):
 *   {"action":"home"}                       — mover a LS_START
 *   {"action":"end"}                        — mover a LS_END
 *   {"action":"impulse","pwm":500,"duration":1.5} — impulso con PWM y duración
 *   {"action":"stop"}                       — freno inmediato
 */
#include "lazo_abierto_mode.h"
#include "hardware_pins.h"
#include "motor_control.h"
#include "encoder_ir.h"
#include "limit_switches.h"
#include <wifi_mqtt_config.h>
#include <cstring>
#include <cstdlib>

namespace LazoAbiertoMode {

// ── Estados ──────────────────────────────────
enum OLState {
    OL_IDLE,
    OL_HOMING_START,     // moviendo a LS_START
    OL_HOMING_END,       // moviendo a LS_END
    OL_READY,            // en posición, listo para impulso
    OL_IMPULSE_RUNNING,  // impulso activo con temporizador
    OL_STOPPED           // impulso completado
};

// ── Variables ────────────────────────────────
static OLState _state = OL_IDLE;
static unsigned long _stateEntry = 0;

// Parámetros del impulso actual
static int _impulsePwm           = 500;
static unsigned long _impulseDurationMs = 1000;

// Posición conocida (para determinar dirección del impulso)
static bool _atStart = false;
static bool _atEnd   = false;

// Flags de comando MQTT
static volatile bool _cmdHome    = false;
static volatile bool _cmdEnd     = false;
static volatile bool _cmdImpulse = false;
static volatile bool _cmdStop    = false;
static int  _pendingPwm          = 500;
static unsigned long _pendingDurationMs = 1000;

// ── Helpers ──────────────────────────────────
static void _changeState(OLState s) {
    _state = s;
    _stateEntry = millis();
}

/**
 * Parsear JSON simplificado (sin parser externo).
 * Busca "key":value o "key":"value" en payload.
 */
static const char* _findValue(const char* json, const char* key) {
    char searchKey[64];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    const char* found = strstr(json, searchKey);
    if (!found) return nullptr;
    found += strlen(searchKey);
    // Saltar espacios y ':'
    while (*found == ' ' || *found == ':') found++;
    return found;
}

static int _parseInt(const char* json, const char* key, int defaultVal) {
    const char* val = _findValue(json, key);
    if (!val) return defaultVal;
    return atoi(val);
}

static float _parseFloat(const char* json, const char* key, float defaultVal) {
    const char* val = _findValue(json, key);
    if (!val) return defaultVal;
    return atof(val);
}

// ── API pública ──────────────────────────────

void begin() {
    _state = OL_IDLE;
    _cmdHome = _cmdEnd = _cmdImpulse = _cmdStop = false;
    _atStart = LimitSwitches::isStartActive();
    _atEnd   = LimitSwitches::isEndActive();
    Serial.println("[openloop] Modo Lazo Abierto activado.");
}

const char* getStateStr() {
    switch (_state) {
        case OL_IDLE:            return "OL_IDLE";
        case OL_HOMING_START:    return "OL_HOMING_START";
        case OL_HOMING_END:      return "OL_HOMING_END";
        case OL_READY:           return "OL_READY";
        case OL_IMPULSE_RUNNING: return "OL_IMPULSE_RUNNING";
        case OL_STOPPED:         return "OL_STOPPED";
        default:                 return "OL_UNKNOWN";
    }
}

bool isActive() {
    return _state == OL_HOMING_START || _state == OL_HOMING_END
        || _state == OL_IMPULSE_RUNNING;
}

void onCommand(const char* topic, const char* payload) {
    // Parada de emergencia global
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        _cmdStop = true;
        return;
    }

    // Solo procesar comandos de openloop
    if (strcmp(topic, TOPIC_CMD_OPENLOOP) != 0) return;

    const char* actionKey = strstr(payload, "\"action\"");
    if (!actionKey) return;

    if (strstr(actionKey, "\"home\"")) {
        _cmdHome = true;
    } else if (strstr(actionKey, "\"end\"")) {
        _cmdEnd = true;
    } else if (strstr(actionKey, "\"impulse\"")) {
        _pendingPwm = _parseInt(payload, "pwm", 500);
        float dur   = _parseFloat(payload, "duration", 1.0f);
        // Clamp valores
        if (_pendingPwm < 200) _pendingPwm = 200;
        if (_pendingPwm > 1023) _pendingPwm = 1023;
        if (dur < 0.1f) dur = 0.1f;
        if (dur > 3.0f) dur = 3.0f;
        _pendingDurationMs = (unsigned long)(dur * 1000.0f);
        _cmdImpulse = true;
    } else if (strstr(actionKey, "\"stop\"")) {
        _cmdStop = true;
    }
}

void loop() {
    unsigned long now = millis();
    bool startActive = LimitSwitches::isStartActive();
    bool endActive   = LimitSwitches::isEndActive();

    // ── Parada de emergencia ──
    if (_cmdStop) {
        _cmdStop = false;
        Motor::freno();
        if (_state == OL_IMPULSE_RUNNING || _state == OL_HOMING_START
            || _state == OL_HOMING_END) {
            _changeState(OL_STOPPED);
        }
        return;
    }

    switch (_state) {

    case OL_IDLE:
    case OL_READY:
    case OL_STOPPED:
        // Actualizar conocimiento de posición
        _atStart = startActive;
        _atEnd   = endActive;

        // Procesar comandos
        if (_cmdHome) {
            _cmdHome = false;
            if (startActive) {
                // Ya estamos en home
                _changeState(OL_READY);
            } else {
                Motor::atras(HOMING_PWM_DEFAULT);
                _changeState(OL_HOMING_START);
            }
        } else if (_cmdEnd) {
            _cmdEnd = false;
            if (endActive) {
                _changeState(OL_READY);
            } else {
                Motor::adelante(HOMING_PWM_DEFAULT);
                _changeState(OL_HOMING_END);
            }
        } else if (_cmdImpulse) {
            _cmdImpulse = false;
            _impulsePwm = _pendingPwm;
            _impulseDurationMs = _pendingDurationMs;

            // Resetear contadores para medición del impulso
            EncoderIR::resetPulseCount();

            // Determinar dirección según posición
            if (startActive || _atStart) {
                // Estamos en inicio → impulso hacia adelante
                Motor::adelante(_impulsePwm);
            } else if (endActive || _atEnd) {
                // Estamos en fin → impulso hacia atrás
                Motor::atras(_impulsePwm);
            } else {
                // Posición desconocida → impulso hacia adelante por defecto
                Motor::adelante(_impulsePwm);
            }
            _changeState(OL_IMPULSE_RUNNING);
            Serial.printf("[openloop] Impulso PWM=%d dur=%lums\n",
                          _impulsePwm, _impulseDurationMs);
        }
        break;

    case OL_HOMING_START:
        if (startActive) {
            Motor::freno();
            _atStart = true;
            _atEnd = false;
            EncoderIR::resetGlobalPosition();
            EncoderIR::resetPulseCount();
            Serial.println("[openloop] Home (inicio) alcanzado.");
            _changeState(OL_READY);
        }
        // Safety timeout (30s)
        if ((now - _stateEntry) > 30000) {
            Motor::freno();
            Serial.println("[openloop] Timeout homing inicio.");
            _changeState(OL_STOPPED);
        }
        break;

    case OL_HOMING_END:
        if (endActive) {
            Motor::freno();
            _atEnd = true;
            _atStart = false;
            EncoderIR::resetGlobalPosition();
            EncoderIR::resetPulseCount();
            Serial.println("[openloop] Home (fin) alcanzado.");
            _changeState(OL_READY);
        }
        // Safety timeout (30s)
        if ((now - _stateEntry) > 30000) {
            Motor::freno();
            Serial.println("[openloop] Timeout homing fin.");
            _changeState(OL_STOPPED);
        }
        break;

    case OL_IMPULSE_RUNNING:
        // Frenar por duración cumplida
        if ((now - _stateEntry) >= _impulseDurationMs) {
            Motor::freno();
            Serial.printf("[openloop] Impulso completado (%lums).\n",
                          _impulseDurationMs);
            _changeState(OL_STOPPED);
            break;
        }
        // Frenar si llega a un limit switch
        if (startActive || endActive) {
            Motor::freno();
            _atStart = startActive;
            _atEnd = endActive;
            Serial.println("[openloop] Impulso detenido por limit switch.");
            _changeState(OL_STOPPED);
        }
        break;
    }
}

}  // namespace LazoAbiertoMode
