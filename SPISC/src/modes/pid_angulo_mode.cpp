/**
 * pid_angulo_mode.cpp — Modo Control PID de Ángulo.
 *
 * Control PID para estabilización angular del péndulo invertido.
 * Convención: 0° = péndulo arriba (vertical), 180° = abajo.
 * Setpoint fijo: 0°/360° (vertical hacia arriba).
 *
 * Estados:
 *   PIDA_IDLE → PIDA_HOMING_START → PIDA_CALIBRATING → PIDA_PREPOSITIONING
 *     → PIDA_READY → PIDA_RUNNING → PIDA_REPOSITIONING → PIDA_READY
 *                                 → PIDA_STOPPED
 *
 * Comandos (via pendulum/cmd/pidang):
 *   {"action":"home"}    — mover a LS_START
 *   {"action":"end"}     — mover a LS_END
 *   {"action":"config","kp":2.0,"ki":0.05,"kd":0.3,"rango_trabajo":10,"tolerancia":2}
 *   {"action":"run"}     — iniciar secuencia completa (homing→calibración→pre-pos→ready)
 *   {"action":"stop"}    — freno inmediato
 *   {"action":"reset"}   — volver a IDLE
 */
#include "pid_angulo_mode.h"
#include "hardware_pins.h"
#include "motor_control.h"
#include "encoder_ir.h"
#include "limit_switches.h"
#include "sensors_i2c.h"
#include "time_sync.h"
#include <wifi_mqtt_config.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

// Macro para logs con timestamp sincronizado
#define LOG_PIDA(fmt, ...) do { \
    char _ts[16]; \
    TimeSync::formatTime(_ts); \
    Serial.printf("[%s][pidang] " fmt, _ts, ##__VA_ARGS__); \
} while(0)

namespace PidAnguloMode {

// ══════════════════════════════════════════════════
// Estados del modo
// ══════════════════════════════════════════════════
enum PidaState {
    PIDA_IDLE,             // Esperando inicio
    PIDA_HOMING_START,     // Moviendo hacia LS_START (home)
    PIDA_HOMING_END,       // Moviendo hacia LS_END
    PIDA_CALIBRATING,      // Auto-calibración de sensores de posición
    PIDA_PREPOSITIONING,   // PID posición interno: mover carro a 45 cm
    PIDA_READY,            // Esperando que usuario lleve péndulo a vertical
    PIDA_RUNNING,          // Control PID ángulo activo
    PIDA_REPOSITIONING,    // Péndulo fuera de rango, reposicionando carro a 45 cm
    PIDA_STOPPED           // Detenido (por comando, límite o error)
};

// ══════════════════════════════════════════════════
// Constantes — PID Ángulo
// ══════════════════════════════════════════════════
static const float DEFAULT_KP             = 19.0f;
static const float DEFAULT_KI             = 0.0f;
static const float DEFAULT_KD             = 0.0f;
static const float DEFAULT_RANGO_TRABAJO  = 20.0f;   // grados desde vertical
static const float DEFAULT_TOLERANCIA     = 2.0f;     // grados

static const float MIN_KP = 0.1f,  MAX_KP = 50.0f;
static const float MIN_KI = 0.0f,  MAX_KI = 10.0f;
static const float MIN_KD = 0.0f,  MAX_KD = 5.0f;
static const float MIN_RANGO_TRABAJO = 5.0f,  MAX_RANGO_TRABAJO = 30.0f;
static const float MIN_TOLERANCIA = 1.0f,  MAX_TOLERANCIA = 10.0f;

static const float INTEGRAL_MAX = 500.0f;           // Anti-windup
static const float DERIV_FILTER_ALPHA = 0.2f;       // Filtro pasa-bajos derivativo

// Rangos PWM configurables
static const int   DEFAULT_PWM_MIN = 300;
static const int   DEFAULT_PWM_MAX = 700;
static const int   ABS_MIN_PWM = 100;
static const int   ABS_MAX_PWM = 1023;

// ══════════════════════════════════════════════════
// Constantes — PID Posición interno (pre-posicionamiento)
// ══════════════════════════════════════════════════
static const float POS_SETPOINT        = 45.0f;    // Centro del riel (cm)
static const float POS_KP              = 25.0f;
static const float POS_KI              = 3.5f;
static const float POS_KD              = 1.2f;
static const float POS_TOLERANCIA      = 1.0f;     // cm
static const float POS_ZONA_SEG        = 5.0f;     // cm desde cada extremo
static const float POS_INTEGRAL_MAX    = 500.0f;
static const int   POS_PWM_MIN         = 300;
static const int   POS_PWM_MAX         = 700;

// Timeouts
static const unsigned long HOMING_TIMEOUT_MS   = 30000;  // 30s
static const unsigned long SETTLE_TIME_MS      = 500;    // Estabilización
static const unsigned long CALIB_DURATION_MS   = 200;    // Pausa post-calibración

// ══════════════════════════════════════════════════
// Variables de estado
// ══════════════════════════════════════════════════
static PidaState _state = PIDA_IDLE;
static unsigned long _stateEntry = 0;

// Posición conocida
static bool _atStart = false;
static bool _atEnd   = false;

// ══════════════════════════════════════════════════
// Parámetros PID Ángulo (configurables via MQTT)
// ══════════════════════════════════════════════════
static float _kp = DEFAULT_KP;
static float _ki = DEFAULT_KI;
static float _kd = DEFAULT_KD;
static float _rangoTrabajo = DEFAULT_RANGO_TRABAJO;
static float _tolerancia = DEFAULT_TOLERANCIA;
static int   _pwmMin = DEFAULT_PWM_MIN;
static int   _pwmMax = DEFAULT_PWM_MAX;

// ══════════════════════════════════════════════════
// Variables del controlador PID Ángulo
// ══════════════════════════════════════════════════
static float _error = 0.0f;
static float _errorPrev = 0.0f;
static float _integral = 0.0f;
static float _derivada = 0.0f;
static float _derivadaFiltrada = 0.0f;
static float _output = 0.0f;
static float _outputSaturated = 0.0f;
static unsigned long _lastControlTime = 0;

// ══════════════════════════════════════════════════
// Variables del controlador PID Posición interno
// ══════════════════════════════════════════════════
static float _posError = 0.0f;
static float _posErrorPrev = 0.0f;
static float _posIntegral = 0.0f;
static float _posDerivada = 0.0f;
static float _posDerivadaFiltrada = 0.0f;
static float _posOutput = 0.0f;
static unsigned long _posLastControlTime = 0;

// Tiempo en zona de tolerancia (para confirmar llegada)
static unsigned long _posInToleranceTime = 0;
static bool _posWasInTolerance = false;

// Rate limiter para debug prints PID posición
static unsigned long _lastPosDebugPrint = 0;

// ══════════════════════════════════════════════════
// Flags de comando MQTT (volatile para cross-core)
// ══════════════════════════════════════════════════
static volatile bool _cmdHome   = false;
static volatile bool _cmdEnd    = false;
static volatile bool _cmdRun    = false;
static volatile bool _cmdStop   = false;
static volatile bool _cmdReset  = false;
static volatile bool _cmdConfig = false;

// Parámetros pendientes de configuración
static float _pendingKp = DEFAULT_KP;
static float _pendingKi = DEFAULT_KI;
static float _pendingKd = DEFAULT_KD;
static float _pendingRangoTrabajo = DEFAULT_RANGO_TRABAJO;
static float _pendingTolerancia = DEFAULT_TOLERANCIA;
static int   _pendingPwmMin = DEFAULT_PWM_MIN;
static int   _pendingPwmMax = DEFAULT_PWM_MAX;

// ══════════════════════════════════════════════════
// Helpers internos
// ══════════════════════════════════════════════════

static void _changeState(PidaState s) {
    _state = s;
    _stateEntry = millis();
}

static void _resetAnglePidVariables() {
    _error = 0.0f;
    _errorPrev = 0.0f;
    _integral = 0.0f;
    _derivada = 0.0f;
    _derivadaFiltrada = 0.0f;
    _output = 0.0f;
    _outputSaturated = 0.0f;
    _lastControlTime = millis();
}

static void _resetPositionPidVariables() {
    _posError = 0.0f;
    _posErrorPrev = 0.0f;
    _posIntegral = 0.0f;
    _posDerivada = 0.0f;
    _posDerivadaFiltrada = 0.0f;
    _posOutput = 0.0f;
    _posLastControlTime = millis();
    _posInToleranceTime = 0;
    _posWasInTolerance = false;
}

/**
 * Normaliza un error angular al rango [-180, +180] grados.
 * Maneja correctamente la transición 360°/0°.
 */
static float _normalizeAngleError(float error) {
    while (error > 180.0f) error -= 360.0f;
    while (error < -180.0f) error += 360.0f;
    return error;
}

/**
 * Verifica si el ángulo del péndulo está dentro del rango de trabajo.
 * Rango se mide desde 0° (vertical arriba).
 * Ej: rango=10 → banda [350°, 10°] (o equivalente: |ángulo| ≤ 10° desde vertical).
 */
static bool _inWorkingRange(float angle, float range) {
    return (angle <= range) || (angle >= (360.0f - range));
}

/**
 * Parseo JSON simplificado (mismo patrón que otros modos).
 */
static const char* _findValue(const char* json, const char* key) {
    char searchKey[64];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    const char* found = strstr(json, searchKey);
    if (!found) return nullptr;
    found += strlen(searchKey);
    while (*found == ' ' || *found == ':') found++;
    return found;
}

static float _parseFloat(const char* json, const char* key, float defaultVal) {
    const char* val = _findValue(json, key);
    if (!val) return defaultVal;
    return atof(val);
}

/**
 * Ejecuta el PID de ángulo y controla el motor.
 * Setpoint siempre es 0° (vertical arriba).
 * El error se normaliza a [-180, +180] para manejar la transición 360°/0°.
 */
static void _executeAnglePidControl(float angleActual) {
    unsigned long now = millis();
    float dt = (now - _lastControlTime) / 1000.0f;
    _lastControlTime = now;

    if (dt < 0.0005f) dt = 0.001f;
    if (dt > 0.1f) dt = 0.1f;

    // 1. Error angular normalizado (setpoint = 0°)
    //    Si ángulo es 358°, error = 0 - 358 = -358 → normalizado: +2° (necesita moverse +2°)
    _error = _normalizeAngleError(0.0f - angleActual);

    // 2. Integral con anti-windup
    _integral += _error * dt;
    if (_integral > INTEGRAL_MAX) _integral = INTEGRAL_MAX;
    if (_integral < -INTEGRAL_MAX) _integral = -INTEGRAL_MAX;

    // 3. Derivada con filtro pasa-bajos
    _derivada = (_error - _errorPrev) / dt;
    _derivadaFiltrada = DERIV_FILTER_ALPHA * _derivada + (1.0f - DERIV_FILTER_ALPHA) * _derivadaFiltrada;
    _errorPrev = _error;

    // 4. Salida PID
    _output = _kp * _error + _ki * _integral + _kd * _derivadaFiltrada;

    // 5. Aplicar control con saturación
    if (fabsf(_error) <= _tolerancia) {
        Motor::freno();
        _outputSaturated = 0.0f;
    } else {
        int pwm = (int)fabsf(_output);
        if (pwm < _pwmMin) pwm = _pwmMin;
        if (pwm > _pwmMax) pwm = _pwmMax;

        _outputSaturated = (_output > 0) ? (float)pwm : -(float)pwm;

        if (_output > 0) {
            Motor::adelante(pwm);
        } else {
            Motor::atras(pwm);
        }
    }
}

/**
 * Ejecuta el PID de posición interno (pre-posicionamiento y reposicionamiento).
 * Usa sensor magnético AS5600 para posición del carro.
 * Retorna true si está dentro de tolerancia.
 */
static bool _executePositionPidControl(float posActual) {
    unsigned long now = millis();
    float dt = (now - _posLastControlTime) / 1000.0f;
    _posLastControlTime = now;

    if (dt < 0.0005f) dt = 0.001f;
    if (dt > 0.1f) dt = 0.1f;

    // 1. Error
    _posError = POS_SETPOINT - posActual;

    // 2. Integral con anti-windup
    _posIntegral += _posError * dt;
    if (_posIntegral > POS_INTEGRAL_MAX) _posIntegral = POS_INTEGRAL_MAX;
    if (_posIntegral < -POS_INTEGRAL_MAX) _posIntegral = -POS_INTEGRAL_MAX;

    // 3. Derivada con filtro pasa-bajos
    _posDerivada = (_posError - _posErrorPrev) / dt;
    _posDerivadaFiltrada = DERIV_FILTER_ALPHA * _posDerivada + (1.0f - DERIV_FILTER_ALPHA) * _posDerivadaFiltrada;
    _posErrorPrev = _posError;

    // 4. Salida PID
    _posOutput = POS_KP * _posError + POS_KI * _posIntegral + POS_KD * _posDerivadaFiltrada;

    // 5. Verificar tolerancia
    bool inTolerance = (fabsf(_posError) <= POS_TOLERANCIA);

    // 6. Aplicar control
    if (inTolerance) {
        Motor::freno();
    } else {
        int pwm = (int)fabsf(_posOutput);
        if (pwm < POS_PWM_MIN) pwm = POS_PWM_MIN;
        if (pwm > POS_PWM_MAX) pwm = POS_PWM_MAX;

        if (_posOutput > 0) {
            Motor::adelante(pwm);
        } else {
            Motor::atras(pwm);
        }
    }

    return inTolerance;
}

// ══════════════════════════════════════════════════
// API Pública
// ══════════════════════════════════════════════════

void begin() {
    _state = PIDA_IDLE;
    _stateEntry = millis();

    // Limpiar flags
    _cmdHome = _cmdEnd = _cmdRun = _cmdStop = _cmdReset = _cmdConfig = false;

    // Leer posición inicial
    _atStart = LimitSwitches::isStartActive();
    _atEnd   = LimitSwitches::isEndActive();

    // Resetear variables PID
    _resetAnglePidVariables();
    _resetPositionPidVariables();

    Serial.println("[pidang] Modo PID Ángulo activado.");

    // Auto-iniciar homing si no está en home
    if (!_atStart) {
        _cmdHome = true;
    } else {
        _changeState(PIDA_READY);
        Serial.println("[pidang] Ya en home, listo para configurar.");
    }
}

const char* getStateStr() {
    switch (_state) {
        case PIDA_IDLE:            return "PIDA_IDLE";
        case PIDA_HOMING_START:    return "PIDA_HOMING_START";
        case PIDA_HOMING_END:      return "PIDA_HOMING_END";
        case PIDA_CALIBRATING:     return "PIDA_CALIBRATING";
        case PIDA_PREPOSITIONING:  return "PIDA_PREPOSITIONING";
        case PIDA_READY:           return "PIDA_READY";
        case PIDA_RUNNING:         return "PIDA_RUNNING";
        case PIDA_REPOSITIONING:   return "PIDA_REPOSITIONING";
        case PIDA_STOPPED:         return "PIDA_STOPPED";
        default:                   return "PIDA_UNKNOWN";
    }
}

bool isActive() {
    return _state == PIDA_HOMING_START || _state == PIDA_HOMING_END
        || _state == PIDA_PREPOSITIONING || _state == PIDA_RUNNING
        || _state == PIDA_REPOSITIONING;
}

void onCommand(const char* topic, const char* payload) {
    // Parada de emergencia global
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        _cmdStop = true;
        return;
    }

    // Solo procesar comandos de PID ángulo
    if (strcmp(topic, TOPIC_CMD_PID_ANG) != 0) return;

    const char* actionKey = strstr(payload, "\"action\"");
    if (!actionKey) return;

    if (strstr(actionKey, "\"home\"")) {
        _cmdHome = true;
    } else if (strstr(actionKey, "\"end\"")) {
        _cmdEnd = true;
    } else if (strstr(actionKey, "\"run\"")) {
        _cmdRun = true;
    } else if (strstr(actionKey, "\"stop\"")) {
        _cmdStop = true;
    } else if (strstr(actionKey, "\"reset\"")) {
        _cmdReset = true;
    } else if (strstr(actionKey, "\"config\"")) {
        // Parsear parámetros de configuración
        _pendingKp = _parseFloat(payload, "kp", _kp);
        _pendingKi = _parseFloat(payload, "ki", _ki);
        _pendingKd = _parseFloat(payload, "kd", _kd);
        _pendingRangoTrabajo = _parseFloat(payload, "rango_trabajo", _rangoTrabajo);
        _pendingTolerancia = _parseFloat(payload, "tolerancia", _tolerancia);
        _pendingPwmMin = (int)_parseFloat(payload, "pwm_min", (float)_pwmMin);
        _pendingPwmMax = (int)_parseFloat(payload, "pwm_max", (float)_pwmMax);

        // Validar y clampar
        if (_pendingKp < MIN_KP) _pendingKp = MIN_KP;
        if (_pendingKp > MAX_KP) _pendingKp = MAX_KP;
        if (_pendingKi < MIN_KI) _pendingKi = MIN_KI;
        if (_pendingKi > MAX_KI) _pendingKi = MAX_KI;
        if (_pendingKd < MIN_KD) _pendingKd = MIN_KD;
        if (_pendingKd > MAX_KD) _pendingKd = MAX_KD;
        if (_pendingRangoTrabajo < MIN_RANGO_TRABAJO) _pendingRangoTrabajo = MIN_RANGO_TRABAJO;
        if (_pendingRangoTrabajo > MAX_RANGO_TRABAJO) _pendingRangoTrabajo = MAX_RANGO_TRABAJO;
        if (_pendingTolerancia < MIN_TOLERANCIA) _pendingTolerancia = MIN_TOLERANCIA;
        if (_pendingTolerancia > MAX_TOLERANCIA) _pendingTolerancia = MAX_TOLERANCIA;

        // Clampar PWM
        if (_pendingPwmMin < ABS_MIN_PWM) _pendingPwmMin = ABS_MIN_PWM;
        if (_pendingPwmMin > 500) _pendingPwmMin = 500;
        if (_pendingPwmMax < 300) _pendingPwmMax = 300;
        if (_pendingPwmMax > ABS_MAX_PWM) _pendingPwmMax = ABS_MAX_PWM;
        if (_pendingPwmMin >= _pendingPwmMax) {
            _pendingPwmMin = DEFAULT_PWM_MIN;
            _pendingPwmMax = DEFAULT_PWM_MAX;
        }

        _cmdConfig = true;
    }
}

void loop() {
    unsigned long now = millis();
    bool startActive = LimitSwitches::isStartActive();
    bool endActive   = LimitSwitches::isEndActive();
    float posActual  = EncoderIR::getPositionCm();
    float angleActual = SensorsI2C::getAnglePend();

    // ══════════════════════════════════════════════════
    // Parada de emergencia (máxima prioridad)
    // ══════════════════════════════════════════════════
    if (_cmdStop) {
        _cmdStop = false;
        Motor::freno();
        _resetAnglePidVariables();
        _resetPositionPidVariables();
        if (_state == PIDA_RUNNING || _state == PIDA_HOMING_START
            || _state == PIDA_HOMING_END || _state == PIDA_PREPOSITIONING
            || _state == PIDA_REPOSITIONING) {
            Serial.println("[pidang] STOP de emergencia.");
            _changeState(PIDA_STOPPED);
        }
        return;
    }

    // ══════════════════════════════════════════════════
    // Reset a IDLE
    // ══════════════════════════════════════════════════
    if (_cmdReset) {
        _cmdReset = false;
        Motor::freno();
        _resetAnglePidVariables();
        _resetPositionPidVariables();
        Serial.println("[pidang] Reset a IDLE.");
        _changeState(PIDA_IDLE);
        return;
    }

    // ══════════════════════════════════════════════════
    // Máquina de estados
    // ══════════════════════════════════════════════════
    switch (_state) {

    case PIDA_IDLE:
    case PIDA_READY:
    case PIDA_STOPPED:
        _atStart = startActive;
        _atEnd   = endActive;

        // Procesar configuración
        if (_cmdConfig) {
            _cmdConfig = false;
            _kp = _pendingKp;
            _ki = _pendingKi;
            _kd = _pendingKd;
            _rangoTrabajo = _pendingRangoTrabajo;
            _tolerancia = _pendingTolerancia;
            _pwmMin = _pendingPwmMin;
            _pwmMax = _pendingPwmMax;

            Serial.printf("[pidang] Config: Kp=%.2f Ki=%.3f Kd=%.3f RT=%.1f° TOL=%.1f°\n",
                _kp, _ki, _kd, _rangoTrabajo, _tolerancia);
            Serial.printf("[pidang] Config: PWM=[%d,%d]\n", _pwmMin, _pwmMax);
        }

        // Procesar comando home
        if (_cmdHome) {
            _cmdHome = false;
            if (startActive) {
                Serial.println("[pidang] Ya en home.");
                _changeState(PIDA_READY);
            } else {
                Motor::atras(HOMING_PWM);
                Serial.println("[pidang] Homing a INICIO...");
                _changeState(PIDA_HOMING_START);
            }
        }
        // Procesar comando end
        else if (_cmdEnd) {
            _cmdEnd = false;
            if (endActive) {
                Serial.println("[pidang] Ya en end.");
                _changeState(PIDA_READY);
            } else {
                Motor::adelante(HOMING_PWM);
                Serial.println("[pidang] Homing a FIN...");
                _changeState(PIDA_HOMING_END);
            }
        }
        // Procesar comando run: inicia la secuencia completa
        else if (_cmdRun) {
            _cmdRun = false;
            Serial.println("[pidang] Iniciando secuencia: Homing → Calibración → Pre-posicionamiento...");
            if (startActive) {
                // Ya en home, pasar a calibración
                _changeState(PIDA_CALIBRATING);
            } else {
                Motor::atras(HOMING_PWM);
                _changeState(PIDA_HOMING_START);
            }
        }

        // En estado READY: verificar si el péndulo entra en rango de trabajo
        if (_state == PIDA_READY) {
            if (_inWorkingRange(angleActual, _rangoTrabajo)) {
                LOG_PIDA("Péndulo en rango de trabajo (ángulo=%.1f°). Activando PID ángulo.\n", angleActual);
                _resetAnglePidVariables();
                _changeState(PIDA_RUNNING);
            }
        }
        break;

    case PIDA_HOMING_START:
        if (startActive) {
            Motor::freno();
            _atStart = true;
            _atEnd = false;
            Serial.println("[pidang] Home (inicio) alcanzado.");
            // Si venimos de un cmd.run, avanzar a calibración
            _changeState(PIDA_CALIBRATING);
            break;
        }
        if ((now - _stateEntry) > HOMING_TIMEOUT_MS) {
            Motor::freno();
            Serial.println("[pidang] Timeout homing inicio.");
            _changeState(PIDA_STOPPED);
        }
        break;

    case PIDA_HOMING_END:
        if (endActive) {
            Motor::freno();
            _atEnd = true;
            _atStart = false;
            Serial.println("[pidang] Home (fin) alcanzado.");
            _changeState(PIDA_READY);
            break;
        }
        if ((now - _stateEntry) > HOMING_TIMEOUT_MS) {
            Motor::freno();
            Serial.println("[pidang] Timeout homing fin.");
            _changeState(PIDA_STOPPED);
        }
        break;

    case PIDA_CALIBRATING:
        // Auto-calibración: LS_START ya calibró posición a 0cm (via main.cpp)
        // Pausa breve para estabilización de sensores
        if ((now - _stateEntry) > CALIB_DURATION_MS) {
            Serial.println("[pidang] Calibración de posición OK. Iniciando pre-posicionamiento a 45 cm...");
            _resetPositionPidVariables();
            _changeState(PIDA_PREPOSITIONING);
        }
        break;

    case PIDA_PREPOSITIONING:
        // Protección de límites físicos
        {
            float posError = POS_SETPOINT - posActual;
            bool blockedByStart = startActive && (posError < 0);
            bool blockedByEnd   = endActive && (posError > 0);
            if (blockedByStart || blockedByEnd) {
                Motor::freno();
                Serial.printf("[pidang] Pre-pos detenido: límite %s activo\n",
                    blockedByStart ? "START" : "END");
                _changeState(PIDA_STOPPED);
                break;
            }
        }

        // Zona de peligro
        if (posActual < POS_ZONA_SEG || posActual > (TRACK_LENGTH_CM - POS_ZONA_SEG)) {
            float posError = POS_SETPOINT - posActual;
            bool goingTowardLimit = (posActual < POS_ZONA_SEG && posError < 0)
                                 || (posActual > (TRACK_LENGTH_CM - POS_ZONA_SEG) && posError > 0);
            if (goingTowardLimit) {
                Motor::freno();
                Serial.printf("[pidang] Pre-pos: zona de peligro (pos=%.1f)\n", posActual);
                _changeState(PIDA_STOPPED);
                break;
            }
        }

        // Ejecutar PID posición
        {
            bool inTolerance = _executePositionPidControl(posActual);

            // Debug print rate-limited (~200ms)
            if ((now - _lastPosDebugPrint) >= 200) {
                _lastPosDebugPrint = now;
                int pwmActual = Motor::getCurrentPWM();
                const char* dir = (_posOutput > 0) ? "ADL" : (_posOutput < 0) ? "ATR" : "DET";
                Serial.printf("[Debug PID Pos] pos=%.2f err=%.2f out=%.1f pwm=%d dir=%s\n",
                    posActual, _posError, _posOutput, pwmActual, dir);
            }

            if (inTolerance) {
                if (!_posWasInTolerance) {
                    _posInToleranceTime = now;
                    _posWasInTolerance = true;
                }
                if ((now - _posInToleranceTime) >= SETTLE_TIME_MS) {
                    Motor::freno();
                    LOG_PIDA("Pre-posicionamiento completado (pos=%.2f cm). Esperando péndulo vertical.\n", posActual);
                    _changeState(PIDA_READY);
                }
            } else {
                _posWasInTolerance = false;
            }
        }
        break;

    case PIDA_RUNNING:
        // Verificar si péndulo salió del rango de trabajo
        if (!_inWorkingRange(angleActual, _rangoTrabajo)) {
            Motor::freno();
            _resetAnglePidVariables();
            LOG_PIDA("Péndulo fuera de rango (ángulo=%.1f°). Reposicionando...\n", angleActual);
            _resetPositionPidVariables();
            _changeState(PIDA_REPOSITIONING);
            break;
        }

        // Protección de límites físicos durante control angular
        {
            bool blockedByStart = startActive && (_output < 0);
            bool blockedByEnd   = endActive && (_output > 0);
            if (blockedByStart || blockedByEnd) {
                Motor::freno();
                _resetAnglePidVariables();
                Serial.printf("[pidang] Control detenido: límite %s activo durante PID ángulo\n",
                    blockedByStart ? "START" : "END");
                _resetPositionPidVariables();
                _changeState(PIDA_REPOSITIONING);
                break;
            }
        }

        // Ejecutar PID de ángulo
        _executeAnglePidControl(angleActual);
        break;

    case PIDA_REPOSITIONING:
        // Retornar carro a 45 cm usando PID posición interno
        // Protección de límites
        {
            float posError = POS_SETPOINT - posActual;
            bool blockedByStart = startActive && (posError < 0);
            bool blockedByEnd   = endActive && (posError > 0);
            if (blockedByStart || blockedByEnd) {
                Motor::freno();
                Serial.printf("[pidang] Reposicionamiento detenido: límite %s activo\n",
                    blockedByStart ? "START" : "END");
                _changeState(PIDA_STOPPED);
                break;
            }
        }

        // Zona de peligro
        if (posActual < POS_ZONA_SEG || posActual > (TRACK_LENGTH_CM - POS_ZONA_SEG)) {
            float posError = POS_SETPOINT - posActual;
            bool goingTowardLimit = (posActual < POS_ZONA_SEG && posError < 0)
                                 || (posActual > (TRACK_LENGTH_CM - POS_ZONA_SEG) && posError > 0);
            if (goingTowardLimit) {
                Motor::freno();
                Serial.printf("[pidang] Reposicionamiento: zona de peligro (pos=%.1f)\n", posActual);
                _changeState(PIDA_STOPPED);
                break;
            }
        }

        // Ejecutar PID posición
        {
            bool inTolerance = _executePositionPidControl(posActual);

            // Debug print rate-limited (~200ms)
            if ((now - _lastPosDebugPrint) >= 200) {
                _lastPosDebugPrint = now;
                int pwmActual = Motor::getCurrentPWM();
                const char* dir = (_posOutput > 0) ? "ADL" : (_posOutput < 0) ? "ATR" : "DET";
                Serial.printf("[Debug PID Pos] pos=%.2f err=%.2f out=%.1f pwm=%d dir=%s (repos)\n",
                    posActual, _posError, _posOutput, pwmActual, dir);
            }

            if (inTolerance) {
                if (!_posWasInTolerance) {
                    _posInToleranceTime = now;
                    _posWasInTolerance = true;
                }
                if ((now - _posInToleranceTime) >= SETTLE_TIME_MS) {
                    Motor::freno();
                    LOG_PIDA("Reposicionamiento completado (pos=%.2f cm). Esperando péndulo vertical.\n", posActual);
                    _changeState(PIDA_READY);
                }
            } else {
                _posWasInTolerance = false;
            }
        }
        break;

    }  // switch(_state)
}

// ══════════════════════════════════════════════════
// Getters para telemetría
// ══════════════════════════════════════════════════

float getError()             { return _error; }
float getIntegral()          { return _integral; }
float getDerivada()          { return _derivadaFiltrada; }
float getOutput()            { return _output; }
float getOutputSaturated()   { return _outputSaturated; }
float getSetpoint()          { return 0.0f; }  // Siempre 0° (vertical)
float getKp()                { return _kp; }
float getKi()                { return _ki; }
float getKd()                { return _kd; }
float getRangoTrabajo()      { return _rangoTrabajo; }
float getTolerancia()        { return _tolerancia; }
int   getPwmMin()            { return _pwmMin; }
int   getPwmMax()            { return _pwmMax; }
float getPosSetpoint()       { return POS_SETPOINT; }

// Getters PID Posición interno
float getPosError()          { return _posError; }
float getPosIntegral()       { return _posIntegral; }
float getPosDerivada()       { return _posDerivadaFiltrada; }
float getPosOutput()         { return _posOutput; }

}  // namespace PidAnguloMode
