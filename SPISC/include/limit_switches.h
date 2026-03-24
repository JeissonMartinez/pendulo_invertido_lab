/**
 * limit_switches.h — Manejo de limit switches XV-152-1C25.
 * ISR con debounce, lectura de estados, freno de seguridad.
 */
#ifndef LIMIT_SWITCHES_H
#define LIMIT_SWITCHES_H

#include <Arduino.h>

namespace LimitSwitches {

/** Inicializa pines INPUT_PULLUP y registra interrupciones. */
void begin();

/** Lee el estado del limit switch de inicio (HIGH = activado). */
bool isStartActive();

/** Lee el estado del limit switch de fin (HIGH = activado). */
bool isEndActive();

/** Retorna true si la ISR de inicio se disparó (y limpia la flag). */
bool wasStartTriggered();

/** Retorna true si la ISR de fin se disparó (y limpia la flag). */
bool wasEndTriggered();

/**
 * Publica por MQTT si el estado de los limit switches cambió.
 * Llamar en cada iteración del loop.
 * @return true si hubo cambio.
 */
bool checkAndPublishChanges();

}  // namespace LimitSwitches

#endif // LIMIT_SWITCHES_H
