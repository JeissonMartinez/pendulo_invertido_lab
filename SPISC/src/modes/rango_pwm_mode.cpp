/**
 * rango_pwm_mode.cpp — Modo Barrido PWM.
 * Lógica extraída de rango_pwm.cpp, usando módulos compartidos.
 */
#include "rango_pwm_mode.h"
#include "hardware_pins.h"
#include "motor_control.h"
#include "encoder_ir.h"
#include "limit_switches.h"
#include "mqtt_comm.h"
#include <wifi_mqtt_config.h>

namespace RangoPwmMode {

// ── Parámetros del barrido ───────────────────
static const int PWM_START          = 400;
static const int PWM_MAX_LIMIT      = 1023;
static const unsigned long NO_MOVE_DETECT_MS = 3000;
static const unsigned long TIMEOUT_BACK_MS   = 10000;
static const unsigned long PAUSE_AT_END_MS   = 2000;
static const unsigned long SETTLE_MS         = 1000;
static const unsigned long PAUSE_AT_START_MS = 2000;

// ── Máximo registros ─────────────────────────
static const int MAX_RECORDS = 1024;

// ── Estados internos ─────────────────────────
enum SweepState {
    SW_IDLE,
    SW_HOMING,
    SW_READY,
    SW_FWD_RUNNING,
    SW_SETTLE_END,
    SW_PAUSE_AT_END,
    SW_BACK_RUNNING,
    SW_SETTLE_START,
    SW_PAUSE_AT_START,
    SW_SHOW_RESULTS,
    SW_WAIT_REINICIAR
};

struct TestRecord {
    int      pwm;
    bool     moved;
    bool     completed;
    uint32_t pulsesFwd;
    uint32_t pulsesBack;
    unsigned long timeFwdMs;
    unsigned long timeBackMs;
    float    rpmFwd;
    float    rpmBack;
};

// ── Variables ────────────────────────────────
static TestRecord records[MAX_RECORDS];
static int recordCount = 0;

static SweepState _state = SW_IDLE;
static unsigned long _stateEntry = 0;

static int _currentPWM = PWM_START;
static int _pwmMin     = -1;
static int _pwmMax     = -1;

static uint32_t _fwdEndPulses  = 0;
static unsigned long _fwdStartTime = 0;
static unsigned long _fwdEndTime   = 0;
static unsigned long _backStartTime = 0;
static bool _fromHoming  = false;
static uint32_t _backEndPulses = 0;
static unsigned long _backEndTime = 0;

static volatile bool _cmdStart     = false;
static volatile bool _cmdStop      = false;
static volatile bool _cmdReiniciar = false;
static bool _stopPending = false;
static bool _pubStarted = false;

static char _jsonBuf[512];

// ── Helpers ──────────────────────────────────
static void _changeState(SweepState s) {
    _state = s;
    _stateEntry = millis();
}

static void _addRecord(int pwm, bool moved, bool completed,
                       uint32_t pFwd, uint32_t pBack,
                       unsigned long tFwd, unsigned long tBack) {
    if (recordCount >= MAX_RECORDS) return;
    TestRecord& r = records[recordCount];
    r.pwm       = pwm;
    r.moved     = moved;
    r.completed = completed;
    r.pulsesFwd  = pFwd;
    r.pulsesBack = pBack;
    r.timeFwdMs  = tFwd;
    r.timeBackMs = tBack;
    r.rpmFwd = (completed && tFwd > 0)
        ? ((float)pFwd / (tFwd / 1000.0f) / PULSES_PER_REV) * 60.0f : 0;
    r.rpmBack = (completed && tBack > 0)
        ? ((float)pBack / (tBack / 1000.0f) / PULSES_PER_REV) * 60.0f : 0;
    recordCount++;
}

// ── Publicación no-bloqueante de resultados ──
static int  _pubIdx = -1;         // -1=summary, 0..N-1=records, N=done
static unsigned long _lastPubMs = 0;
static const unsigned long PUB_INTERVAL_MS = 30;  // un mensaje cada 30ms

/** Inicia la publicación (llamar una vez al entrar en SW_SHOW_RESULTS). */
static void _startPublishResults() {
    _pubIdx = -1;
    _lastPubMs = 0;  // forzar primer envío inmediato
}

/**
 * Publica UN mensaje por iteración de loop.
 * Retorna true cuando ha terminado de publicar todo.
 */
static bool _publishResultsTick() {
    unsigned long now = millis();
    if ((now - _lastPubMs) < PUB_INTERVAL_MS) return false;
    _lastPubMs = now;

    if (_pubIdx == -1) {
        // Summary
        snprintf(_jsonBuf, sizeof(_jsonBuf),
            "{\"type\":\"summary\",\"pwmMin\":%d,\"pwmMax\":%d,"
            "\"totalRecords\":%d,\"rangoUtil\":%d}",
            _pwmMin, _pwmMax, recordCount,
            (_pwmMin > 0 && _pwmMax > 0) ? (_pwmMax - _pwmMin + 1) : 0);
        MqttComm::publish(TOPIC_SWEEP_RESULTS, _jsonBuf);
        _pubIdx = 0;
        return false;
    }

    if (_pubIdx < recordCount) {
        TestRecord& r = records[_pubIdx];
        snprintf(_jsonBuf, sizeof(_jsonBuf),
            "{\"type\":\"record\",\"idx\":%d,\"pwm\":%d,"
            "\"moved\":%s,\"completed\":%s,"
            "\"pulsesFwd\":%lu,\"pulsesBack\":%lu,"
            "\"timeFwdMs\":%lu,\"timeBackMs\":%lu,"
            "\"rpmFwd\":%.1f,\"rpmBack\":%.1f}",
            _pubIdx, r.pwm,
            r.moved     ? "true" : "false",
            r.completed ? "true" : "false",
            (unsigned long)r.pulsesFwd, (unsigned long)r.pulsesBack,
            r.timeFwdMs, r.timeBackMs,
            r.rpmFwd, r.rpmBack);
        MqttComm::publish(TOPIC_SWEEP_RESULTS, _jsonBuf);
        _pubIdx++;
        return false;
    }

    // Done
    snprintf(_jsonBuf, sizeof(_jsonBuf),
        "{\"type\":\"done\",\"totalRecords\":%d}", recordCount);
    MqttComm::publish(TOPIC_SWEEP_RESULTS, _jsonBuf);
    return true;
}

static void _resetSweep() {
    _currentPWM   = PWM_START;
    _pwmMin       = -1;
    _pwmMax       = -1;
    recordCount   = 0;
    _cmdStop      = false;
    _cmdStart     = false;
    _cmdReiniciar = false;
    _stopPending  = false;
    _pubStarted   = false;
}

// ── API pública ──────────────────────────────

void begin() {
    _resetSweep();
    EncoderIR::resetGlobalPosition();
    _changeState(SW_IDLE);
    Serial.println("[sweep] Modo barrido PWM activado.");
}

const char* getStateStr() {
    switch (_state) {
        case SW_IDLE:           return "IDLE";
        case SW_HOMING:         return "HOMING";
        case SW_READY:          return "READY";
        case SW_FWD_RUNNING:    return "FWD_RUNNING";
        case SW_SETTLE_END:     return "SETTLE_END";
        case SW_PAUSE_AT_END:   return "PAUSE_AT_END";
        case SW_BACK_RUNNING:   return "BACK_RUNNING";
        case SW_SETTLE_START:   return "SETTLE_START";
        case SW_PAUSE_AT_START: return "PAUSE_AT_START";
        case SW_SHOW_RESULTS:   return "SHOW_RESULTS";
        case SW_WAIT_REINICIAR: return "WAIT_REINICIAR";
        default:                return "UNKNOWN";
    }
}

bool isActive() {
    return _state != SW_IDLE && _state != SW_WAIT_REINICIAR;
}

void onCommand(const char* topic, const char* payload) {
    if (strcmp(topic, TOPIC_CMD_STOP) == 0) {
        _cmdStop = true;
        return;
    }
    if (strcmp(topic, TOPIC_CMD_SWEEP) != 0) return;

    const char* actionKey = strstr(payload, "\"action\"");
    if (!actionKey) return;

    if (strstr(actionKey, "\"start\""))       _cmdStart = true;
    else if (strstr(actionKey, "\"stop\""))    _cmdStop = true;
    else if (strstr(actionKey, "\"reiniciar\"")) _cmdReiniciar = true;
}

void loop() {
    unsigned long now = millis();
    bool startActive = LimitSwitches::isStartActive();
    bool endActive   = LimitSwitches::isEndActive();

    // ── Parada de emergencia ──
    if (_cmdStop) {
        _cmdStop = false;
        if (_state != SW_IDLE && _state != SW_SHOW_RESULTS
            && _state != SW_WAIT_REINICIAR) {
            Motor::freno();
            if (_pwmMax < 0)
                _pwmMax = _currentPWM > PWM_START ? _currentPWM - 1 : _currentPWM;
            Motor::atras(HOMING_PWM);
            _changeState(SW_HOMING);
            _fromHoming = true;
            _stopPending = true;
            return;
        }
        Motor::freno();
    }

    switch (_state) {

    case SW_IDLE:
        if (_cmdStart) {
            _cmdStart = false;
            _resetSweep();
            if (startActive) {
                _changeState(SW_READY);
            } else {
                Motor::atras(HOMING_PWM);
                _changeState(SW_HOMING);
            }
        }
        break;

    case SW_HOMING:
        if (startActive) {
            _fromHoming = true;
            _changeState(SW_SETTLE_START);
        }
        break;

    case SW_READY:
        if (_currentPWM > PWM_MAX_LIMIT) {
            _changeState(SW_SHOW_RESULTS);
            break;
        }
        EncoderIR::resetPulseCount();
        _fwdStartTime = now;
        Motor::adelante(_currentPWM);
        _changeState(SW_FWD_RUNNING);
        break;

    case SW_FWD_RUNNING:
        if (endActive) {
            _fwdEndPulses = EncoderIR::getPulseCount();
            _fwdEndTime = now;
            if (_pwmMin < 0) _pwmMin = _currentPWM;
            _changeState(SW_SETTLE_END);
            break;
        }
        if ((now - _fwdStartTime) >= NO_MOVE_DETECT_MS && EncoderIR::getPulseCount() < 3) {
            Motor::freno();
            _addRecord(_currentPWM, false, false, 0, 0, 0, 0);
            _currentPWM++;
            _changeState(SW_READY);
        }
        break;

    case SW_SETTLE_END:
        if ((now - _stateEntry) >= SETTLE_MS) {
            Motor::freno();
            _changeState(SW_PAUSE_AT_END);
        }
        break;

    case SW_PAUSE_AT_END:
        if ((now - _stateEntry) >= PAUSE_AT_END_MS) {
            EncoderIR::resetPulseCount();
            _backStartTime = now;
            Motor::atras(_currentPWM);
            _changeState(SW_BACK_RUNNING);
        }
        break;

    case SW_BACK_RUNNING:
        if (startActive) {
            _backEndPulses = EncoderIR::getPulseCount();
            _backEndTime = now;
            _fromHoming = false;
            _changeState(SW_SETTLE_START);
            break;
        }
        if ((now - _backStartTime) >= TIMEOUT_BACK_MS) {
            Motor::freno();
            uint32_t backPulses = EncoderIR::getPulseCount();
            unsigned long backTime = now - _backStartTime;
            unsigned long fwdTime  = _fwdEndTime - _fwdStartTime;
            _addRecord(_currentPWM, true, true, _fwdEndPulses, backPulses, fwdTime, backTime);
            _currentPWM++;
            _fromHoming = true;
            Motor::atras(HOMING_PWM);
            _changeState(SW_HOMING);
        }
        break;

    case SW_SETTLE_START:
        if ((now - _stateEntry) >= SETTLE_MS) {
            Motor::freno();
            EncoderIR::resetGlobalPosition();
            EncoderIR::resetPulseCount();

            if (_fromHoming) {
                if (_stopPending) {
                    _stopPending = false;
                    _changeState(SW_SHOW_RESULTS);
                } else {
                    _changeState(SW_READY);
                }
            } else {
                unsigned long fwdTime = _fwdEndTime - _fwdStartTime;
                unsigned long backTime = _backEndTime - _backStartTime;
                _addRecord(_currentPWM, true, true,
                           _fwdEndPulses, _backEndPulses, fwdTime, backTime);
                _currentPWM++;
                _changeState(SW_PAUSE_AT_START);
            }
        }
        break;

    case SW_PAUSE_AT_START:
        if ((now - _stateEntry) >= PAUSE_AT_START_MS) {
            _changeState(SW_READY);
        }
        break;

    case SW_SHOW_RESULTS:
        if (!_pubStarted) {
            _startPublishResults();
            _pubStarted = true;
        }
        if (_publishResultsTick()) {
            _pubStarted = false;
            _changeState(SW_WAIT_REINICIAR);
        }
        break;

    case SW_WAIT_REINICIAR:
        if (_cmdReiniciar || _cmdStart) {
            _cmdReiniciar = false;
            _cmdStart = false;
            _resetSweep();
            if (startActive) {
                _changeState(SW_READY);
            } else {
                Motor::atras(HOMING_PWM);
                _changeState(SW_HOMING);
            }
        }
        break;
    }
}

}  // namespace RangoPwmMode
