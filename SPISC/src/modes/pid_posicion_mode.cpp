/**
 * pid_posicion_mode.cpp — Modo Control PID de Posición.
 *
 * Estados: PID_IDLE → PID_HOMING_START → PID_READY → PID_RUNNING → PID_AT_TARGET / PID_STOPPED
 *
 * Comandos (via pendulum/cmd/pidpos):
 *   {"action":"home"}                                              — mover a LS_START
 *   {"action":"end"}                                               — mover a LS_END
 *   {"action":"config","kp":2.0,"ki":0.1,"kd":0.05,"setpoint":45,"zona_seguridad":5}
 *   {"action":"run"}                                               — iniciar control PID
 *   {"action":"stop"}                                              — freno inmediato
 *   {"action":"reset"}                                             — volver a IDLE
 */
#include "pid_posicion_mode.h"
#include "hardware_pins.h"
#include "motor_control.h"
#include "encoder_ir.h"
#include "limit_switches.h"
#include "sensors_i2c.h"
#include <wifi_mqtt_config.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace PidPosicionMode {

// ══════════════════════════════════════════════════
// Estados del modo
// ══════════════════════════════════════════════════
enum PidState {
    PID_IDLE,           // Esperando inicio
    PID_HOMING_START,   // Moviendo hacia LS_START (home)
    PID_HOMING_END,     // Moviendo hacia LS_END
    PID_READY,          // En posición inicial, listo para configurar y ejecutar
    PID_RUNNING,        // Control PID activo
    PID_AT_TARGET,      // Setpoint alcanzado, esperando nuevo comando
    PID_STOPPED         // Detenido (por comando, límite o error)
};

// ══════════════════════════════════════════════════
// Constantes de control
// ══════════════════════════════════════════════════
static const float DEFAULT_KP           = 8.0f;    // Ganancia proporcional
static const float DEFAULT_KI           = 0.1f;    // Ganancia integral
static const float DEFAULT_KD           = 0.5f;    // Ganancia derivativa
static const float DEFAULT_SETPOINT     = 45.0f;   // Centro del riel (cm)
static const float DEFAULT_ZONA_SEG     = 5.0f;    // Zona de seguridad (cm)
static const float DEFAULT_TOLERANCIA   = 1.0f;    // Tolerancia de llegada (cm)

static const float MIN_KP = 0.1f,  MAX_KP = 50.0f;
static const float MIN_KI = 0.0f,  MAX_KI = 10.0f;
static const float MIN_KD = 0.0f,  MAX_KD = 5.0f;
static const float MIN_ZONA_SEG = 2.0f,  MAX_ZONA_SEG = 20.0f;
static const float MIN_TOLERANCIA = 0.5f, MAX_TOLERANCIA = 5.0f;

static const float INTEGRAL_MAX = 500.0f;   // Anti-windup: límite de integral

// Rangos para PWM configurable
static const int   DEFAULT_PWM_MIN = 200;   // PWM mínimo por defecto
static const int   DEFAULT_PWM_MAX = 700;   // PWM máximo por defecto
static const int   ABS_MIN_PWM = 100;       // Límite absoluto mínimo
static const int   ABS_MAX_PWM = 1023;      // Límite absoluto máximo

static const float DERIV_FILTER_ALPHA = 0.2f;  // Filtro pasa-bajos derivativo (0-1)

static const unsigned long HOMING_TIMEOUT_MS = 30000;  // Timeout de homing (30s)
static const unsigned long SETTLE_TIME_MS    = 500;    // Tiempo de estabilización en setpoint

// ══════════════════════════════════════════════════
// Variables de estado
// ══════════════════════════════════════════════════
static PidState _state = PID_IDLE;
static unsigned long _stateEntry = 0;

// Posición conocida
static bool _atStart = false;
static bool _atEnd   = false;

// ══════════════════════════════════════════════════
// Parámetros PID (configurables via MQTT)
// ══════════════════════════════════════════════════
static float _kp = DEFAULT_KP;
static float _ki = DEFAULT_KI;
static float _kd = DEFAULT_KD;
static float _setpoint = DEFAULT_SETPOINT;
static float _zonaSeguridad = DEFAULT_ZONA_SEG;
static float _tolerancia = DEFAULT_TOLERANCIA;

// Selección de sensor: true = AS5600 magnético, false = Encoder IR
static bool _useMagSensor = true;

// PWM configurable
static int _pwmMin = DEFAULT_PWM_MIN;
static int _pwmMax = DEFAULT_PWM_MAX;

// ══════════════════════════════════════════════════
// Variables del controlador PID
// ══════════════════════════════════════════════════
static float _error = 0.0f;
static float _errorPrev = 0.0f;
static float _integral = 0.0f;
static float _derivada = 0.0f;
static float _derivadaFiltrada = 0.0f;
static float _output = 0.0f;        // Salida PID cruda (antes de saturación)
static float _outputSaturated = 0.0f;  // Salida PID saturada (PWM aplicado)

// Tiempo para control discreto
static unsigned long _lastControlTime = 0;

// Tiempo en zona de tolerancia (para confirmar llegada)
static unsigned long _inToleranceTime = 0;
static bool _wasInTolerance = false;

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
static float _pendingSetpoint = DEFAULT_SETPOINT;
static float _pendingZonaSeg = DEFAULT_ZONA_SEG;
static float _pendingTolerancia = DEFAULT_TOLERANCIA;
static bool  _pendingUseMagSensor = true;
static int   _pendingPwmMin = DEFAULT_PWM_MIN;
static int   _pendingPwmMax = DEFAULT_PWM_MAX;

// ══════════════════════════════════════════════════
// Helpers internos
// ══════════════════════════════════════════════════

static void _changeState(PidState s) {
    _state = s;
    _stateEntry = millis();
}

static void _resetPidVariables() {
    _error = 0.0f;
    _errorPrev = 0.0f;
    _integral = 0.0f;
    _derivada = 0.0f;
    _derivadaFiltrada = 0.0f;
    _output = 0.0f;
    _outputSaturated = 0.0f;
    _lastControlTime = millis();
    _inToleranceTime = 0;
    _wasInTolerance = false;
}

/**
 * Verifica si un setpoint está dentro de la zona segura.
 * Zona segura = [zona_seguridad, TRACK_LENGTH_CM - zona_seguridad]
 */
static bool _isSetpointValid(float sp, float zonaSeg) {
    return (sp >= zonaSeg) && (sp <= (TRACK_LENGTH_CM - zonaSeg));
}

/**
 * Verifica si la posición actual está en zona de peligro (cerca de límites).
 */
static bool _inDangerZone(float pos, float zonaSeg) {
    return (pos < zonaSeg) || (pos > (TRACK_LENGTH_CM - zonaSeg));
}

/**
 * Parseo JSON simplificado (reutilizado de lazo_abierto_mode).
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
 * Aplica el algoritmo PID y controla el motor.
 * Retorna true si está dentro de tolerancia del setpoint.
 */
static bool _executePidControl(float posActual) {
    unsigned long now = millis();
    float dt = (now - _lastControlTime) / 1000.0f;  // Convertir a segundos
    _lastControlTime = now;
    
    // Evitar división por cero o dt muy pequeño
    if (dt < 0.0005f) dt = 0.001f;  // Mínimo 0.5ms
    if (dt > 0.1f) dt = 0.1f;       // Máximo 100ms (prevenir saltos grandes)

    // 1. Calcular error
    _error = _setpoint - posActual;

    // 2. Término integral con anti-windup
    _integral += _error * dt;
    if (_integral > INTEGRAL_MAX) _integral = INTEGRAL_MAX;
    if (_integral < -INTEGRAL_MAX) _integral = -INTEGRAL_MAX;

    // 3. Término derivativo con filtro pasa-bajos
    _derivada = (_error - _errorPrev) / dt;
    _derivadaFiltrada = DERIV_FILTER_ALPHA * _derivada + (1.0f - DERIV_FILTER_ALPHA) * _derivadaFiltrada;
    _errorPrev = _error;

    // 4. Calcular salida PID (cruda, antes de saturación)
    _output = _kp * _error + _ki * _integral + _kd * _derivadaFiltrada;

    // 5. Verificar si está en tolerancia
    bool inTolerance = (fabsf(_error) <= _tolerancia);

    // 6. Aplicar control al motor con saturación configurable
    if (inTolerance) {
        // Dentro de tolerancia: frenar
        Motor::freno();
        _outputSaturated = 0.0f;
    } else {
        // Fuera de tolerancia: aplicar PWM
        int pwm = (int)fabsf(_output);
        
        // Saturar PWM con límites configurables
        if (pwm < _pwmMin) pwm = _pwmMin;
        if (pwm > _pwmMax) pwm = _pwmMax;
        
        // Guardar salida saturada (con signo para indicar dirección)
        _outputSaturated = (_output > 0) ? (float)pwm : -(float)pwm;

        if (_output > 0) {
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
    // Resetear estado
    _state = PID_IDLE;
    _stateEntry = millis();
    
    // Limpiar flags
    _cmdHome = _cmdEnd = _cmdRun = _cmdStop = _cmdReset = _cmdConfig = false;
    
    // Leer posición inicial
    _atStart = LimitSwitches::isStartActive();
    _atEnd   = LimitSwitches::isEndActive();
    
    // Resetear variables PID
    _resetPidVariables();
    
    Serial.println("[pidpos] Modo PID Posición activado.");
    
    // Auto-iniciar homing si no está en home
    if (!_atStart) {
        _cmdHome = true;
    } else {
        _changeState(PID_READY);
        Serial.println("[pidpos] Ya en home, listo para configurar.");
    }
}

const char* getStateStr() {
    switch (_state) {
        case PID_IDLE:         return "PID_IDLE";
        case PID_HOMING_START: return "PID_HOMING_START";
        case PID_HOMING_END:   return "PID_HOMING_END";
        case PID_READY:        return "PID_READY";
        case PID_RUNNING:      return "PID_RUNNING";
        case PID_AT_TARGET:    return "PID_AT_TARGET";
        case PID_STOPPED:      return "PID_STOPPED";
        default:               return "PID_UNKNOWN";
    }
}

bool isActive() {
    return _state == PID_HOMING_START || _state == PID_HOMING_END 
        || _state == PID_RUNNING;
}

void onCommand(const char* topic, const char* payload) {
    // Parada de emergencia global
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        _cmdStop = true;
        return;
    }

    // Solo procesar comandos de PID posición
    if (strcmp(topic, TOPIC_CMD_PID_POS) != 0) return;

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
        _pendingSetpoint = _parseFloat(payload, "setpoint", _setpoint);
        _pendingZonaSeg = _parseFloat(payload, "zona_seguridad", _zonaSeguridad);
        _pendingTolerancia = _parseFloat(payload, "tolerancia", _tolerancia);
        
        // Parsear selección de sensor
        const char* sensorVal = _findValue(payload, "sensor");
        if (sensorVal) {
            if (strstr(sensorVal, "\"ir\"")) {
                _pendingUseMagSensor = false;
            } else if (strstr(sensorVal, "\"mag\"")) {
                _pendingUseMagSensor = true;
            }
        } else {
            _pendingUseMagSensor = _useMagSensor;  // Mantener actual
        }
        
        // Parsear límites PWM
        _pendingPwmMin = (int)_parseFloat(payload, "pwm_min", (float)_pwmMin);
        _pendingPwmMax = (int)_parseFloat(payload, "pwm_max", (float)_pwmMax);
        
        // Validar y clampar valores
        if (_pendingKp < MIN_KP) _pendingKp = MIN_KP;
        if (_pendingKp > MAX_KP) _pendingKp = MAX_KP;
        if (_pendingKi < MIN_KI) _pendingKi = MIN_KI;
        if (_pendingKi > MAX_KI) _pendingKi = MAX_KI;
        if (_pendingKd < MIN_KD) _pendingKd = MIN_KD;
        if (_pendingKd > MAX_KD) _pendingKd = MAX_KD;
        if (_pendingZonaSeg < MIN_ZONA_SEG) _pendingZonaSeg = MIN_ZONA_SEG;
        if (_pendingZonaSeg > MAX_ZONA_SEG) _pendingZonaSeg = MAX_ZONA_SEG;
        if (_pendingTolerancia < MIN_TOLERANCIA) _pendingTolerancia = MIN_TOLERANCIA;
        if (_pendingTolerancia > MAX_TOLERANCIA) _pendingTolerancia = MAX_TOLERANCIA;
        
        // Clampar PWM
        if (_pendingPwmMin < ABS_MIN_PWM) _pendingPwmMin = ABS_MIN_PWM;
        if (_pendingPwmMin > 500) _pendingPwmMin = 500;
        if (_pendingPwmMax < 300) _pendingPwmMax = 300;
        if (_pendingPwmMax > ABS_MAX_PWM) _pendingPwmMax = ABS_MAX_PWM;
        // Asegurar que min < max
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
    
    // Leer posición según sensor seleccionado
    float posActual = _useMagSensor ? SensorsI2C::getPosMagCm() : EncoderIR::getPositionCm();

    // ══════════════════════════════════════════════════
    // Parada de emergencia (máxima prioridad)
    // ══════════════════════════════════════════════════
    if (_cmdStop) {
        _cmdStop = false;
        Motor::freno();
        _resetPidVariables();
        if (_state == PID_RUNNING || _state == PID_HOMING_START 
            || _state == PID_HOMING_END) {
            Serial.println("[pidpos] STOP de emergencia.");
            _changeState(PID_STOPPED);
        }
        return;
    }

    // ══════════════════════════════════════════════════
    // Reset a IDLE
    // ══════════════════════════════════════════════════
    if (_cmdReset) {
        _cmdReset = false;
        Motor::freno();
        _resetPidVariables();
        Serial.println("[pidpos] Reset a IDLE.");
        _changeState(PID_IDLE);
        return;
    }

    // ══════════════════════════════════════════════════
    // Máquina de estados
    // ══════════════════════════════════════════════════
    switch (_state) {

    case PID_IDLE:
    case PID_READY:
    case PID_STOPPED:
    case PID_AT_TARGET:
        // Actualizar conocimiento de posición
        _atStart = startActive;
        _atEnd   = endActive;

        // Procesar configuración
        if (_cmdConfig) {
            _cmdConfig = false;
            
            // Validar setpoint dentro de zona segura
            if (!_isSetpointValid(_pendingSetpoint, _pendingZonaSeg)) {
                Serial.printf("[pidpos] ERROR: Setpoint %.1f fuera de zona segura [%.1f, %.1f]\n",
                    _pendingSetpoint, _pendingZonaSeg, TRACK_LENGTH_CM - _pendingZonaSeg);
                // No aplicar configuración inválida
            } else {
                _kp = _pendingKp;
                _ki = _pendingKi;
                _kd = _pendingKd;
                _setpoint = _pendingSetpoint;
                _zonaSeguridad = _pendingZonaSeg;
                _tolerancia = _pendingTolerancia;
                _useMagSensor = _pendingUseMagSensor;
                _pwmMin = _pendingPwmMin;
                _pwmMax = _pendingPwmMax;
                
                Serial.printf("[pidpos] Config: Kp=%.2f Ki=%.3f Kd=%.3f SP=%.1f ZS=%.1f TOL=%.1f\n",
                    _kp, _ki, _kd, _setpoint, _zonaSeguridad, _tolerancia);
                Serial.printf("[pidpos] Config: Sensor=%s PWM=[%d,%d]\n",
                    _useMagSensor ? "MAG" : "IR", _pwmMin, _pwmMax);
            }
        }

        // Procesar comando home
        if (_cmdHome) {
            _cmdHome = false;
            if (startActive) {
                Serial.println("[pidpos] Ya en home.");
                _changeState(PID_READY);
            } else {
                Motor::atras(HOMING_PWM);
                Serial.println("[pidpos] Homing a INICIO...");
                _changeState(PID_HOMING_START);
            }
        }
        // Procesar comando end
        else if (_cmdEnd) {
            _cmdEnd = false;
            if (endActive) {
                Serial.println("[pidpos] Ya en end.");
                _changeState(PID_READY);
            } else {
                Motor::adelante(HOMING_PWM);
                Serial.println("[pidpos] Homing a FIN...");
                _changeState(PID_HOMING_END);
            }
        }
        // Procesar comando run
        else if (_cmdRun) {
            _cmdRun = false;
            
            // Validar que el setpoint sea válido
            if (!_isSetpointValid(_setpoint, _zonaSeguridad)) {
                Serial.printf("[pidpos] ERROR: Setpoint %.1f inválido. Configurar primero.\n", _setpoint);
                _changeState(PID_STOPPED);
            } else {
                // Resetear variables del PID antes de iniciar
                _resetPidVariables();
                Serial.printf("[pidpos] Iniciando control PID → SP=%.1f cm\n", _setpoint);
                _changeState(PID_RUNNING);
            }
        }
        break;

    case PID_HOMING_START:
        if (startActive) {
            Motor::freno();
            _atStart = true;
            _atEnd = false;
            Serial.println("[pidpos] Home (inicio) alcanzado.");
            _changeState(PID_READY);
            break;
        }
        // Timeout de seguridad
        if ((now - _stateEntry) > HOMING_TIMEOUT_MS) {
            Motor::freno();
            Serial.println("[pidpos] Timeout homing inicio.");
            _changeState(PID_STOPPED);
        }
        break;

    case PID_HOMING_END:
        if (endActive) {
            Motor::freno();
            _atEnd = true;
            _atStart = false;
            Serial.println("[pidpos] Home (fin) alcanzado.");
            _changeState(PID_READY);
            break;
        }
        // Timeout de seguridad
        if ((now - _stateEntry) > HOMING_TIMEOUT_MS) {
            Motor::freno();
            Serial.println("[pidpos] Timeout homing fin.");
            _changeState(PID_STOPPED);
        }
        break;

    case PID_RUNNING:
        // Verificar límites físicos (seguridad máxima)
        if (startActive || endActive) {
            Motor::freno();
            _atStart = startActive;
            _atEnd = endActive;
            Serial.println("[pidpos] Control detenido por limit switch.");
            _changeState(PID_STOPPED);
            break;
        }

        // Verificar zona de peligro (aproximación a límites)
        if (_inDangerZone(posActual, _zonaSeguridad)) {
            // Solo frenar si nos alejamos del setpoint hacia el límite
            float errorActual = _setpoint - posActual;
            bool goingTowardLimit = (posActual < _zonaSeguridad && errorActual < 0) 
                                 || (posActual > (TRACK_LENGTH_CM - _zonaSeguridad) && errorActual > 0);
            if (goingTowardLimit) {
                Motor::freno();
                Serial.printf("[pidpos] Zona de peligro detectada (pos=%.1f).\n", posActual);
                _changeState(PID_STOPPED);
                break;
            }
        }

        // Ejecutar control PID
        {
            bool inTolerance = _executePidControl(posActual);
            
            if (inTolerance) {
                // Verificar tiempo de estabilización
                if (!_wasInTolerance) {
                    _inToleranceTime = now;
                    _wasInTolerance = true;
                }
                
                if ((now - _inToleranceTime) >= SETTLE_TIME_MS) {
                    Motor::freno();
                    Serial.printf("[pidpos] Setpoint alcanzado (pos=%.2f, SP=%.2f, err=%.3f)\n",
                        posActual, _setpoint, _error);
                    _changeState(PID_AT_TARGET);
                }
            } else {
                _wasInTolerance = false;
            }
        }
        break;
    }  // switch(_state)
}

// ══════════════════════════════════════════════════
// Getters para telemetría
// ══════════════════════════════════════════════════

float getError()          { return _error; }
float getIntegral()       { return _integral; }
float getDerivada()       { return _derivadaFiltrada; }
float getOutput()         { return _output; }           // Salida cruda (alias)
float getOutputRaw()      { return _output; }           // Salida antes de saturación
float getOutputSaturated(){ return _outputSaturated; }  // Salida después de saturación
float getSetpoint()       { return _setpoint; }
float getKp()             { return _kp; }
float getKi()             { return _ki; }
float getKd()             { return _kd; }
float getZonaSeguridad()  { return _zonaSeguridad; }
float getTolerancia()     { return _tolerancia; }
int   getSensorType()     { return _useMagSensor ? 0 : 1; }  // 0=mag, 1=ir
int   getPwmMin()         { return _pwmMin; }
int   getPwmMax()         { return _pwmMax; }

}  // namespace PidPosicionMode
