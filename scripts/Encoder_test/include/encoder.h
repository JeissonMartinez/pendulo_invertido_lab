#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

void     encoder_init();
void     encoder_update();       // Llamar periódicamente para recalcular velocidad
uint32_t encoder_getPulseCount();
float    encoder_getTurns();
float    encoder_getRPS();
float    encoder_getRPM();

#endif // ENCODER_H
