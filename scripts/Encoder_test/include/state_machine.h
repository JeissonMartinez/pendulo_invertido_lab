#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <Arduino.h>

enum MotorState {
    STATE_STOP_INIT,
    STATE_FWD_SLOW,
    STATE_FWD_MED,
    STATE_FWD_FAST,
    STATE_FWD_MAX,
    STATE_STOP_MID,
    STATE_REV_SLOW,
    STATE_REV_MED,
    STATE_REV_FAST,
    STATE_REV_MAX,
    STATE_STOP_END,
    STATE_COUNT       // Cantidad total de estados
};

void        sm_init();
void        sm_update();
MotorState  sm_getCurrentState();
const char* sm_getStateName();

#endif // STATE_MACHINE_H
