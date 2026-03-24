/**
 * limit_switches.cpp — Implementación de limit switches.
 */
#include "limit_switches.h"
#include "hardware_pins.h"

namespace LimitSwitches {

static volatile bool _startTriggered = false;
static volatile bool _endTriggered   = false;
static volatile unsigned long _lastISR_start = 0;
static volatile unsigned long _lastISR_end   = 0;

static bool _prevStart = false;
static bool _prevEnd   = false;

static void IRAM_ATTR _isrStart() {
    unsigned long now = micros();
    if ((now - _lastISR_start) > LS_DEBOUNCE_US) {
        _startTriggered = true;
        _lastISR_start  = now;
    }
}

static void IRAM_ATTR _isrEnd() {
    unsigned long now = micros();
    if ((now - _lastISR_end) > LS_DEBOUNCE_US) {
        _endTriggered = true;
        _lastISR_end  = now;
    }
}

void begin() {
    pinMode(PIN_LIMIT_START, INPUT_PULLUP);
    pinMode(PIN_LIMIT_END,   INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_START), _isrStart, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_END),   _isrEnd,   RISING);
    _prevStart = isStartActive();
    _prevEnd   = isEndActive();
}

bool isStartActive() { return digitalRead(PIN_LIMIT_START) == HIGH; }
bool isEndActive()   { return digitalRead(PIN_LIMIT_END)   == HIGH; }

bool wasStartTriggered() {
    if (_startTriggered) {
        _startTriggered = false;
        return true;
    }
    return false;
}

bool wasEndTriggered() {
    if (_endTriggered) {
        _endTriggered = false;
        return true;
    }
    return false;
}

bool checkAndPublishChanges() {
    bool s = isStartActive();
    bool e = isEndActive();
    bool changed = (s != _prevStart || e != _prevEnd);
    _prevStart = s;
    _prevEnd   = e;
    return changed;
}

}  // namespace LimitSwitches
