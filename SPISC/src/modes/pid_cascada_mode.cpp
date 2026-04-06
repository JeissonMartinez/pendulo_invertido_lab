/**
 * pid_cascada_mode.cpp — Modo Control PID en Cascada.
 *
 * Control PID cascada para el péndulo invertido:
 *   - Lazo externo (posición): PID pos → θ_ref (saturado a ±theta_ref_max)
 *   - Lazo interno (ángulo):   PID ang con setpoint = θ_ref → PWM al motor
 *
 * Convención: 0° = péndulo arriba (vertical), 180° = abajo.
 *
 * Estados:
 *   PIDC_IDLE → PIDC_HOMING_START → PIDC_CALIBRATING → PIDC_WAIT_READY
 *     → PIDC_READY → PIDC_RUNNING → PIDC_STOPPED
 *
 * Comandos (via pendulum/cmd/pidcasc):
 *   {"action":"home"}    — mover a LS_START
 *   {"action":"end"}     — mover a LS_END
 *   {"action":"config","kp_pos":...,"kp_ang":...,...}
 *   {"action":"run"}     — iniciar secuencia completa
 *   {"action":"stop"}    — freno inmediato
 *   {"action":"reset"}   — volver a IDLE
 */
#include "pid_cascada_mode.h"
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
#define LOG_PIDC(fmt, ...) do { \
    char _ts[16]; \
    TimeSync::formatTime(_ts); \
    Serial.printf("[%s][pidcasc] " fmt, _ts, ##__VA_ARGS__); \
} while(0)

namespace PidCascadaMode {

// ══════════════════════════════════════════════════
// Estados del modo
// ══════════════════════════════════════════════════
enum PidcState {
    PIDC_IDLE,             // Esperando inicio
    PIDC_HOMING_START,     // Moviendo hacia LS_START (home)
    PIDC_HOMING_END,       // Moviendo hacia LS_END
    PIDC_CALIBRATING,      // Auto-calibración de sensores de posición
    PIDC_WAIT_READY,       // Esperando 2s post-home para estabilización
    PIDC_READY,            // Esperando botón EJECUTAR del usuario
    PIDC_RUNNING,          // Control cascada activo
    PIDC_STOPPED           // Detenido (por comando, límite o error)
};

// ══════════════════════════════════════════════════
// Constantes — PID Ángulo (lazo interno)
// ══════════════════════════════════════════════════
static const float DEFAULT_ANG_KP         = 40.0f;
static const float DEFAULT_ANG_KI         = 1.0f;
static const float DEFAULT_ANG_KD         = 0.5f;
static const float DEFAULT_RANGO_TRABAJO  = 20.0f;   // grados desde vertical
static const float DEFAULT_TOLERANCIA_ANG = 0.1f;     // grados

static const float MIN_ANG_KP = 0.1f,  MAX_ANG_KP = 100.0f;
static const float MIN_ANG_KI = 0.0f,  MAX_ANG_KI = 50.0f;
static const float MIN_ANG_KD = 0.0f,  MAX_ANG_KD = 10.0f;
static const float MIN_RANGO_TRABAJO = 5.0f,  MAX_RANGO_TRABAJO = 30.0f;
static const float MIN_TOLERANCIA_ANG = 0.0f, MAX_TOLERANCIA_ANG = 5.0f;

static const float ANG_INTEGRAL_MAX = 500.0f;
static const float DERIV_FILTER_ALPHA = 0.2f;

// Rangos PWM configurables
static const int   DEFAULT_PWM_MIN = 250;
static const int   DEFAULT_PWM_MAX = 900;
static const int   ABS_MIN_PWM = 100;
static const int   ABS_MAX_PWM = 1023;

// ══════════════════════════════════════════════════
// Constantes — PID Posición (lazo externo)
// ══════════════════════════════════════════════════
static const float DEFAULT_POS_KP         = 0.5f;
static const float DEFAULT_POS_KI         = 0.01f;
static const float DEFAULT_POS_KD         = 0.1f;
static const float DEFAULT_POS_SETPOINT   = 45.0f;    // Centro del riel (cm)
static const float DEFAULT_POS_ZONA_SEG   = 5.0f;     // cm desde cada extremo
static const float DEFAULT_THETA_REF_MAX  = 2.0f;     // ±grados de saturación

static const float MIN_POS_KP = 0.0f,  MAX_POS_KP = 20.0f;
static const float MIN_POS_KI = 0.0f,  MAX_POS_KI = 5.0f;
static const float MIN_POS_KD = 0.0f,  MAX_POS_KD = 5.0f;
static const float MIN_POS_SETPOINT = 5.0f;
static const float MIN_POS_ZONA_SEG = 2.0f, MAX_POS_ZONA_SEG = 20.0f;
static const float MIN_THETA_REF_MAX = 0.5f, MAX_THETA_REF_MAX = 10.0f;

static const float POS_INTEGRAL_MAX = 500.0f;

// Timeouts
static const unsigned long HOMING_TIMEOUT_MS   = 30000;  // 30s
static const unsigned long CALIB_DURATION_MS   = 200;    // Pausa post-calibración
static const unsigned long WAIT_READY_MS       = 2000;   // 2s espera antes de READY

// ══════════════════════════════════════════════════
// Variables de estado
// ══════════════════════════════════════════════════
static PidcState _state = PIDC_IDLE;
static unsigned long _stateEntry = 0;

// Posición conocida
static bool _atStart = false;
static bool _atEnd   = false;

// ══════════════════════════════════════════════════
// Parámetros PID Ángulo — lazo interno (configurables via MQTT)
// ══════════════════════════════════════════════════
static float _angKp = DEFAULT_ANG_KP;
static float _angKi = DEFAULT_ANG_KI;
static float _angKd = DEFAULT_ANG_KD;
static float _rangoTrabajo = DEFAULT_RANGO_TRABAJO;
static float _toleranciaAng = DEFAULT_TOLERANCIA_ANG;
static int   _pwmMin = DEFAULT_PWM_MIN;
static int   _pwmMax = DEFAULT_PWM_MAX;

// ══════════════════════════════════════════════════
// Parámetros PID Posición — lazo externo (configurables via MQTT)
// ══════════════════════════════════════════════════
static float _posKp = DEFAULT_POS_KP;
static float _posKi = DEFAULT_POS_KI;
static float _posKd = DEFAULT_POS_KD;
static float _posSetpoint = DEFAULT_POS_SETPOINT;
static float _posZonaSeg = DEFAULT_POS_ZONA_SEG;
static float _thetaRefMax = DEFAULT_THETA_REF_MAX;

// ══════════════════════════════════════════════════
// Variables del controlador — Lazo interno (Ángulo)
// ══════════════════════════════════════════════════
static float _angError = 0.0f;
static float _angErrorPrev = 0.0f;
static float _angIntegral = 0.0f;
static float _angDerivada = 0.0f;
static float _angDerivadaFiltrada = 0.0f;
static float _angOutput = 0.0f;
static float _angOutputSaturated = 0.0f;
static float _angSetpoint = 0.0f;       // θ_ref del lazo externo
static unsigned long _angLastControlTime = 0;

// ══════════════════════════════════════════════════
// Variables del controlador — Lazo externo (Posición)
// ══════════════════════════════════════════════════
static float _posError = 0.0f;
static float _posErrorPrev = 0.0f;
static float _posIntegral = 0.0f;
static float _posDerivada = 0.0f;
static float _posDerivadaFiltrada = 0.0f;
static float _posOutput = 0.0f;
static float _posOutputSaturated = 0.0f;  // θ_ref saturado
static unsigned long _posLastControlTime = 0;

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
static float _pendingAngKp = DEFAULT_ANG_KP;
static float _pendingAngKi = DEFAULT_ANG_KI;
static float _pendingAngKd = DEFAULT_ANG_KD;
static float _pendingPosKp = DEFAULT_POS_KP;
static float _pendingPosKi = DEFAULT_POS_KI;
static float _pendingPosKd = DEFAULT_POS_KD;
static float _pendingPosSetpoint = DEFAULT_POS_SETPOINT;
static float _pendingRangoTrabajo = DEFAULT_RANGO_TRABAJO;
static float _pendingToleranciaAng = DEFAULT_TOLERANCIA_ANG;
static float _pendingPosZonaSeg = DEFAULT_POS_ZONA_SEG;
static float _pendingThetaRefMax = DEFAULT_THETA_REF_MAX;
static int   _pendingPwmMin = DEFAULT_PWM_MIN;
static int   _pendingPwmMax = DEFAULT_PWM_MAX;

// ══════════════════════════════════════════════════
// Helpers internos
// ══════════════════════════════════════════════════

static void _changeState(PidcState s) {
    _state = s;
    _stateEntry = millis();
}

static void _resetAnglePidVariables() {
    _angError = 0.0f;
    _angErrorPrev = 0.0f;
    _angIntegral = 0.0f;
    _angDerivada = 0.0f;
    _angDerivadaFiltrada = 0.0f;
    _angOutput = 0.0f;
    _angOutputSaturated = 0.0f;
    _angSetpoint = 0.0f;
    _angLastControlTime = millis();
}

static void _resetPositionPidVariables() {
    _posError = 0.0f;
    _posErrorPrev = 0.0f;
    _posIntegral = 0.0f;
    _posDerivada = 0.0f;
    _posDerivadaFiltrada = 0.0f;
    _posOutput = 0.0f;
    _posOutputSaturated = 0.0f;
    _posLastControlTime = millis();
}

/**
 * Normaliza un error angular al rango [-180, +180] grados.
 */
static float _normalizeAngleError(float error) {
    while (error > 180.0f) error -= 360.0f;
    while (error < -180.0f) error += 360.0f;
    return error;
}

/**
 * Verifica si el ángulo del péndulo está dentro del rango de trabajo.
 * Rango se mide desde 0° (vertical arriba).
 */
static bool _inWorkingRange(float angle, float range) {
    return (angle <= range) || (angle >= (360.0f - range));
}

/**
 * Parseo JSON simplificado.
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
 * Ejecuta el lazo externo (posición) del control cascada.
 * La salida es θ_ref (ángulo de referencia para el lazo interno),
 * saturado a ±_thetaRefMax grados.
 */
static void _executePositionLoop(float posActual) {
    unsigned long now = millis();
    float dt = (now - _posLastControlTime) / 1000.0f;
    _posLastControlTime = now;

    if (dt < 0.0005f) dt = 0.001f;
    if (dt > 0.1f) dt = 0.1f;

    // 1. Error de posición
    _posError = _posSetpoint - posActual;

    // 2. Integral con anti-windup
    _posIntegral += _posError * dt;
    if (_posIntegral > POS_INTEGRAL_MAX) _posIntegral = POS_INTEGRAL_MAX;
    if (_posIntegral < -POS_INTEGRAL_MAX) _posIntegral = -POS_INTEGRAL_MAX;

    // 3. Derivada con filtro pasa-bajos
    _posDerivada = (_posError - _posErrorPrev) / dt;
    _posDerivadaFiltrada = DERIV_FILTER_ALPHA * _posDerivada + (1.0f - DERIV_FILTER_ALPHA) * _posDerivadaFiltrada;
    _posErrorPrev = _posError;

    // 4. Salida PID (θ_ref crudo)
    _posOutput = _posKp * _posError + _posKi * _posIntegral + _posKd * _posDerivadaFiltrada;

    // 5. Saturar salida a ±theta_ref_max grados
    _posOutputSaturated = _posOutput;
    if (_posOutputSaturated > _thetaRefMax) _posOutputSaturated = _thetaRefMax;
    if (_posOutputSaturated < -_thetaRefMax) _posOutputSaturated = -_thetaRefMax;
}

/**
 * Ejecuta el lazo interno (ángulo) del control cascada.
 * El setpoint es θ_ref (viene del lazo externo).
 * La salida controla el motor directamente.
 */
static void _executeAngleLoop(float angleActual) {
    unsigned long now = millis();
    float dt = (now - _angLastControlTime) / 1000.0f;
    _angLastControlTime = now;

    if (dt < 0.0005f) dt = 0.001f;
    if (dt > 0.1f) dt = 0.1f;

    // El setpoint del lazo interno es θ_ref del lazo externo
    _angSetpoint = _posOutputSaturated;

    // 1. Error angular normalizado (setpoint = θ_ref)
    _angError = _normalizeAngleError(_angSetpoint - angleActual);

    // 2. Integral con anti-windup
    _angIntegral += _angError * dt;
    if (_angIntegral > ANG_INTEGRAL_MAX) _angIntegral = ANG_INTEGRAL_MAX;
    if (_angIntegral < -ANG_INTEGRAL_MAX) _angIntegral = -ANG_INTEGRAL_MAX;

    // 3. Derivada con filtro pasa-bajos
    _angDerivada = (_angError - _angErrorPrev) / dt;
    _angDerivadaFiltrada = DERIV_FILTER_ALPHA * _angDerivada + (1.0f - DERIV_FILTER_ALPHA) * _angDerivadaFiltrada;
    _angErrorPrev = _angError;

    // 4. Salida PID
    _angOutput = _angKp * _angError + _angKi * _angIntegral + _angKd * _angDerivadaFiltrada;

    // 5. Aplicar control con saturación
    if (fabsf(_angError) <= _toleranciaAng) {
        Motor::freno();
        _angOutputSaturated = 0.0f;
    } else {
        int pwm = (int)fabsf(_angOutput) + MOTOR_BIAS;
        if (pwm > _pwmMax) pwm = _pwmMax;

        _angOutputSaturated = (_angOutput > 0) ? (float)pwm : -(float)pwm;

        if (_angOutput > 0) {
            Motor::adelante(pwm);
        } else {
            Motor::atras(pwm);
        }
    }
}

// ══════════════════════════════════════════════════
// API Pública
// ══════════════════════════════════════════════════

void begin() {
    _state = PIDC_IDLE;
    _stateEntry = millis();

    // Limpiar flags
    _cmdHome = _cmdEnd = _cmdRun = _cmdStop = _cmdReset = _cmdConfig = false;

    // Leer posición inicial
    _atStart = LimitSwitches::isStartActive();
    _atEnd   = LimitSwitches::isEndActive();

    // Resetear variables PID
    _resetAnglePidVariables();
    _resetPositionPidVariables();

    Serial.println("[pidcasc] Modo PID Cascada activado.");

    // Auto-iniciar homing si no está en home
    if (!_atStart) {
        _cmdHome = true;
    } else {
        _changeState(PIDC_CALIBRATING);
        Serial.println("[pidcasc] Ya en home, calibrando...");
    }
}

const char* getStateStr() {
    switch (_state) {
        case PIDC_IDLE:            return "PIDC_IDLE";
        case PIDC_HOMING_START:    return "PIDC_HOMING_START";
        case PIDC_HOMING_END:      return "PIDC_HOMING_END";
        case PIDC_CALIBRATING:     return "PIDC_CALIBRATING";
        case PIDC_WAIT_READY:      return "PIDC_WAIT_READY";
        case PIDC_READY:           return "PIDC_READY";
        case PIDC_RUNNING:         return "PIDC_RUNNING";
        case PIDC_STOPPED:         return "PIDC_STOPPED";
        default:                   return "PIDC_UNKNOWN";
    }
}

bool isActive() {
    return _state == PIDC_HOMING_START || _state == PIDC_HOMING_END
        || _state == PIDC_RUNNING;
}

void onCommand(const char* topic, const char* payload) {
    // Parada de emergencia global
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        _cmdStop = true;
        return;
    }

    // Solo procesar comandos de PID cascada
    if (strcmp(topic, TOPIC_CMD_PID_CASC) != 0) return;

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
        // Parsear parámetros de configuración — lazo externo (posición)
        _pendingPosKp = _parseFloat(payload, "kp_pos", _posKp);
        _pendingPosKi = _parseFloat(payload, "ki_pos", _posKi);
        _pendingPosKd = _parseFloat(payload, "kd_pos", _posKd);
        _pendingPosSetpoint = _parseFloat(payload, "pos_setpoint", _posSetpoint);
        _pendingPosZonaSeg = _parseFloat(payload, "zona_seguridad", _posZonaSeg);
        _pendingThetaRefMax = _parseFloat(payload, "theta_ref_max", _thetaRefMax);

        // Parsear parámetros — lazo interno (ángulo)
        _pendingAngKp = _parseFloat(payload, "kp_ang", _angKp);
        _pendingAngKi = _parseFloat(payload, "ki_ang", _angKi);
        _pendingAngKd = _parseFloat(payload, "kd_ang", _angKd);
        _pendingRangoTrabajo = _parseFloat(payload, "rango_trabajo", _rangoTrabajo);
        _pendingToleranciaAng = _parseFloat(payload, "tolerancia_ang", _toleranciaAng);

        // PWM motor
        _pendingPwmMin = (int)_parseFloat(payload, "pwm_min", (float)_pwmMin);
        _pendingPwmMax = (int)_parseFloat(payload, "pwm_max", (float)_pwmMax);

        // Validar y clampar — lazo posición
        if (_pendingPosKp < MIN_POS_KP) _pendingPosKp = MIN_POS_KP;
        if (_pendingPosKp > MAX_POS_KP) _pendingPosKp = MAX_POS_KP;
        if (_pendingPosKi < MIN_POS_KI) _pendingPosKi = MIN_POS_KI;
        if (_pendingPosKi > MAX_POS_KI) _pendingPosKi = MAX_POS_KI;
        if (_pendingPosKd < MIN_POS_KD) _pendingPosKd = MIN_POS_KD;
        if (_pendingPosKd > MAX_POS_KD) _pendingPosKd = MAX_POS_KD;
        if (_pendingPosZonaSeg < MIN_POS_ZONA_SEG) _pendingPosZonaSeg = MIN_POS_ZONA_SEG;
        if (_pendingPosZonaSeg > MAX_POS_ZONA_SEG) _pendingPosZonaSeg = MAX_POS_ZONA_SEG;
        // Validar setpoint dentro de zona segura
        float maxSetpoint = TRACK_LENGTH_CM - _pendingPosZonaSeg;
        if (_pendingPosSetpoint < _pendingPosZonaSeg) _pendingPosSetpoint = _pendingPosZonaSeg;
        if (_pendingPosSetpoint > maxSetpoint) _pendingPosSetpoint = maxSetpoint;
        if (_pendingThetaRefMax < MIN_THETA_REF_MAX) _pendingThetaRefMax = MIN_THETA_REF_MAX;
        if (_pendingThetaRefMax > MAX_THETA_REF_MAX) _pendingThetaRefMax = MAX_THETA_REF_MAX;

        // Validar y clampar — lazo ángulo
        if (_pendingAngKp < MIN_ANG_KP) _pendingAngKp = MIN_ANG_KP;
        if (_pendingAngKp > MAX_ANG_KP) _pendingAngKp = MAX_ANG_KP;
        if (_pendingAngKi < MIN_ANG_KI) _pendingAngKi = MIN_ANG_KI;
        if (_pendingAngKi > MAX_ANG_KI) _pendingAngKi = MAX_ANG_KI;
        if (_pendingAngKd < MIN_ANG_KD) _pendingAngKd = MIN_ANG_KD;
        if (_pendingAngKd > MAX_ANG_KD) _pendingAngKd = MAX_ANG_KD;
        if (_pendingRangoTrabajo < MIN_RANGO_TRABAJO) _pendingRangoTrabajo = MIN_RANGO_TRABAJO;
        if (_pendingRangoTrabajo > MAX_RANGO_TRABAJO) _pendingRangoTrabajo = MAX_RANGO_TRABAJO;
        if (_pendingToleranciaAng < MIN_TOLERANCIA_ANG) _pendingToleranciaAng = MIN_TOLERANCIA_ANG;
        if (_pendingToleranciaAng > MAX_TOLERANCIA_ANG) _pendingToleranciaAng = MAX_TOLERANCIA_ANG;

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
        if (_state == PIDC_RUNNING || _state == PIDC_HOMING_START
            || _state == PIDC_HOMING_END) {
            Serial.println("[pidcasc] STOP de emergencia.");
            _changeState(PIDC_STOPPED);
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
        Serial.println("[pidcasc] Reset a IDLE.");
        _changeState(PIDC_IDLE);
        return;
    }

    // ══════════════════════════════════════════════════
    // Máquina de estados
    // ══════════════════════════════════════════════════
    switch (_state) {

    case PIDC_IDLE:
    case PIDC_READY:
    case PIDC_STOPPED:
        _atStart = startActive;
        _atEnd   = endActive;

        // Procesar configuración (solo en estados de motor detenido)
        if (_cmdConfig) {
            _cmdConfig = false;
            _posKp = _pendingPosKp;
            _posKi = _pendingPosKi;
            _posKd = _pendingPosKd;
            _posSetpoint = _pendingPosSetpoint;
            _posZonaSeg = _pendingPosZonaSeg;
            _thetaRefMax = _pendingThetaRefMax;
            _angKp = _pendingAngKp;
            _angKi = _pendingAngKi;
            _angKd = _pendingAngKd;
            _rangoTrabajo = _pendingRangoTrabajo;
            _toleranciaAng = _pendingToleranciaAng;
            _pwmMin = _pendingPwmMin;
            _pwmMax = _pendingPwmMax;

            Serial.printf("[pidcasc] Config Pos: Kp=%.3f Ki=%.4f Kd=%.3f SP=%.1fcm ZS=%.1fcm θmax=%.1f°\n",
                _posKp, _posKi, _posKd, _posSetpoint, _posZonaSeg, _thetaRefMax);
            Serial.printf("[pidcasc] Config Ang: Kp=%.2f Ki=%.3f Kd=%.3f RT=%.1f° TOL=%.1f°\n",
                _angKp, _angKi, _angKd, _rangoTrabajo, _toleranciaAng);
            Serial.printf("[pidcasc] Config Motor: PWM=[%d,%d]\n", _pwmMin, _pwmMax);
        }

        // Procesar comando home
        if (_cmdHome) {
            _cmdHome = false;
            if (startActive) {
                Serial.println("[pidcasc] Ya en home.");
                if (_state == PIDC_IDLE) {
                    _changeState(PIDC_CALIBRATING);
                }
            } else {
                Motor::atras(HOMING_PWM);
                Serial.println("[pidcasc] Homing a INICIO...");
                _changeState(PIDC_HOMING_START);
            }
        }
        // Procesar comando end
        else if (_cmdEnd) {
            _cmdEnd = false;
            if (endActive) {
                Serial.println("[pidcasc] Ya en end.");
            } else {
                Motor::adelante(HOMING_PWM);
                Serial.println("[pidcasc] Homing a FIN...");
                _changeState(PIDC_HOMING_END);
            }
        }
        // Procesar comando run
        else if (_cmdRun) {
            _cmdRun = false;
            Serial.println("[pidcasc] Iniciando secuencia: Homing → Calibración → Espera → Ready...");
            if (startActive) {
                // Ya en home, pasar a calibración
                _changeState(PIDC_CALIBRATING);
            } else {
                Motor::atras(HOMING_PWM);
                _changeState(PIDC_HOMING_START);
            }
        }

        // En estado READY: NO auto-transiciona (espera botón EJECUTAR explícito)
        // El usuario debe enviar cmd:run desde READY para iniciar RUNNING
        if (_state == PIDC_READY && _cmdRun) {
            _cmdRun = false;
            if (_inWorkingRange(angleActual, _rangoTrabajo)) {
                LOG_PIDC("Péndulo en rango de trabajo (ángulo=%.1f°). Activando control cascada.\n", angleActual);
                _resetAnglePidVariables();
                _resetPositionPidVariables();
                _changeState(PIDC_RUNNING);
            } else {
                LOG_PIDC("Péndulo FUERA de rango de trabajo (ángulo=%.1f°). Lleve el péndulo a vertical.\n", angleActual);
            }
        }
        break;

    case PIDC_HOMING_START:
        if (startActive) {
            Motor::freno();
            _atStart = true;
            _atEnd = false;
            Serial.println("[pidcasc] Home (inicio) alcanzado.");
            _changeState(PIDC_CALIBRATING);
            break;
        }
        if ((now - _stateEntry) > HOMING_TIMEOUT_MS) {
            Motor::freno();
            Serial.println("[pidcasc] Timeout homing inicio.");
            _changeState(PIDC_STOPPED);
        }
        break;

    case PIDC_HOMING_END:
        if (endActive) {
            Motor::freno();
            _atEnd = true;
            _atStart = false;
            Serial.println("[pidcasc] Home (fin) alcanzado.");
            _changeState(PIDC_STOPPED);
            break;
        }
        if ((now - _stateEntry) > HOMING_TIMEOUT_MS) {
            Motor::freno();
            Serial.println("[pidcasc] Timeout homing fin.");
            _changeState(PIDC_STOPPED);
        }
        break;

    case PIDC_CALIBRATING:
        // Auto-calibración: LS_START ya calibró posición a 0cm (via main.cpp)
        // Pausa breve para estabilización de sensores
        if ((now - _stateEntry) > CALIB_DURATION_MS) {
            Serial.println("[pidcasc] Calibración OK. Esperando 2s para estabilización...");
            _changeState(PIDC_WAIT_READY);
        }
        break;

    case PIDC_WAIT_READY:
        // Esperar 2 segundos para estabilización
        if ((now - _stateEntry) > WAIT_READY_MS) {
            Serial.println("[pidcasc] Estabilización completa. LISTO para control cascada.");
            _changeState(PIDC_READY);
        }
        break;

    case PIDC_RUNNING:
        // 1. Verificar si péndulo salió del rango de trabajo
        if (!_inWorkingRange(angleActual, _rangoTrabajo)) {
            Motor::freno();
            _resetAnglePidVariables();
            _resetPositionPidVariables();
            LOG_PIDC("Péndulo fuera de rango (ángulo=%.1f°). Motor detenido.\n", angleActual);
            _changeState(PIDC_STOPPED);
            break;
        }

        // 2. Protección de límites físicos
        if (startActive || endActive) {
            Motor::freno();
            _resetAnglePidVariables();
            _resetPositionPidVariables();
            Serial.printf("[pidcasc] Límite %s alcanzado. Motor detenido.\n",
                startActive ? "START" : "END");
            _changeState(PIDC_STOPPED);
            break;
        }

        // 3. Verificar zona de seguridad de posición
        if (posActual < _posZonaSeg || posActual > (TRACK_LENGTH_CM - _posZonaSeg)) {
            Motor::freno();
            _resetAnglePidVariables();
            _resetPositionPidVariables();
            Serial.printf("[pidcasc] Zona de seguridad (pos=%.1f cm). Motor detenido.\n", posActual);
            _changeState(PIDC_STOPPED);
            break;
        }

        // 4. Ejecutar control cascada
        //    Lazo externo: PID posición → θ_ref
        _executePositionLoop(posActual);
        //    Lazo interno: PID ángulo con setpoint = θ_ref → PWM
        _executeAngleLoop(angleActual);
        break;

    } // end switch
}

// ══════════════════════════════════════════════════
// Getters para Telemetría — Lazo externo (Posición)
// ══════════════════════════════════════════════════

float getPosError()          { return _posError; }
float getPosIntegral()       { return _posIntegral; }
float getPosDerivada()       { return _posDerivadaFiltrada; }
float getPosOutput()         { return _posOutput; }
float getPosOutputSaturated(){ return _posOutputSaturated; }
float getPosSetpoint()       { return _posSetpoint; }
float getPosKp()             { return _posKp; }
float getPosKi()             { return _posKi; }
float getPosKd()             { return _posKd; }
float getPosZonaSeguridad()  { return _posZonaSeg; }

// ══════════════════════════════════════════════════
// Getters para Telemetría — Lazo interno (Ángulo)
// ══════════════════════════════════════════════════

float getAngError()          { return _angError; }
float getAngIntegral()       { return _angIntegral; }
float getAngDerivada()       { return _angDerivadaFiltrada; }
float getAngOutput()         { return _angOutput; }
float getAngOutputSaturated(){ return _angOutputSaturated; }
float getAngSetpoint()       { return _angSetpoint; }
float getAngKp()             { return _angKp; }
float getAngKi()             { return _angKi; }
float getAngKd()             { return _angKd; }

// ══════════════════════════════════════════════════
// Getters para Telemetría — Parámetros comunes
// ══════════════════════════════════════════════════

float getRangoTrabajo()      { return _rangoTrabajo; }
float getToleranciaAng()     { return _toleranciaAng; }
int   getPwmMin()            { return _pwmMin; }
int   getPwmMax()            { return _pwmMax; }
float getThetaRefMax()       { return _thetaRefMax; }

} // namespace PidCascadaMode
