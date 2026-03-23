#include "state_machine.h"
#include "config.h"
#include "motor.h"

static MotorState currentState = STATE_STOP_INIT;
static unsigned long stateEntryTime = 0;

// Nombres legibles de cada estado
static const char* stateNames[] = {
    "STOP_INIT",
    "FWD_SLOW",
    "FWD_MED",
    "FWD_FAST",
    "FWD_MAX",
    "STOP_MID",
    "REV_SLOW",
    "REV_MED",
    "REV_FAST",
    "REV_MAX",
    "STOP_END"
};

// Aplicar la acción correspondiente al entrar a un estado
static void applyState(MotorState state) {
    switch (state) {
        case STATE_STOP_INIT:
        case STATE_STOP_MID:
        case STATE_STOP_END:
            motor_stop();
            break;

        case STATE_FWD_SLOW:
            motor_forward();
            motor_setSpeed(SPEED_VALUES[1]);  // 85
            break;
        case STATE_FWD_MED:
            motor_forward();
            motor_setSpeed(SPEED_VALUES[2]);  // 170
            break;
        case STATE_FWD_FAST:
            motor_forward();
            motor_setSpeed(SPEED_VALUES[3]);  // 212
            break;
        case STATE_FWD_MAX:
            motor_forward();
            motor_setSpeed(SPEED_VALUES[4]);  // 255
            break;

        case STATE_REV_SLOW:
            motor_reverse();
            motor_setSpeed(SPEED_VALUES[1]);  // 85
            break;
        case STATE_REV_MED:
            motor_reverse();
            motor_setSpeed(SPEED_VALUES[2]);  // 170
            break;
        case STATE_REV_FAST:
            motor_reverse();
            motor_setSpeed(SPEED_VALUES[3]);  // 212
            break;
        case STATE_REV_MAX:
            motor_reverse();
            motor_setSpeed(SPEED_VALUES[4]);  // 255
            break;

        default:
            motor_stop();
            break;
    }
}

void sm_init() {
    currentState   = STATE_STOP_INIT;
    stateEntryTime = millis();
    applyState(currentState);
}

void sm_update() {
    if (millis() - stateEntryTime >= STATE_DURATION_MS) {
        // Avanzar al siguiente estado (ciclar)
        int next = (int)currentState + 1;
        if (next >= STATE_COUNT) {
            next = 0;  // Reiniciar ciclo
        }
        currentState   = (MotorState)next;
        stateEntryTime = millis();
        applyState(currentState);
    }
}

MotorState sm_getCurrentState() {
    return currentState;
}

const char* sm_getStateName() {
    return stateNames[currentState];
}
