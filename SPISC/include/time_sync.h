/**
 * time_sync.h — Sincronización de tiempo entre ESP32 y servidor
 * 
 * El servidor envía su timestamp Unix (ms) y el ESP32 calcula el offset
 * para mostrar logs con el mismo tiempo que el CSV.
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>

namespace TimeSync {

/**
 * Actualiza el offset de tiempo con el timestamp del servidor.
 * @param serverTimeMs Timestamp Unix del servidor en milisegundos
 */
void setServerTime(uint64_t serverTimeMs);

/**
 * Obtiene el tiempo actual sincronizado en milisegundos Unix.
 * Si no hay sincronización, retorna millis().
 */
uint64_t now();

/**
 * Formatea el tiempo actual como HH:MM:SS.mmm
 * @param buffer Buffer de al menos 13 caracteres
 */
void formatTime(char* buffer);

/**
 * Retorna true si el tiempo está sincronizado con el servidor.
 */
bool isSynced();

}  // namespace TimeSync

#endif // TIME_SYNC_H
