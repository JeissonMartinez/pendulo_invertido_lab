/**
 * time_sync.cpp — Implementación de sincronización de tiempo
 */

#include "time_sync.h"

namespace TimeSync {

static uint64_t _serverTimeAtSync = 0;   // Timestamp del servidor cuando llegó sync
static uint32_t _localMillisAtSync = 0;  // millis() local cuando llegó sync
static bool _synced = false;

void setServerTime(uint64_t serverTimeMs) {
    _serverTimeAtSync = serverTimeMs;
    _localMillisAtSync = millis();
    _synced = true;
    
    // Log de confirmación
    char buf[16];
    formatTime(buf);
    Serial.printf("[TimeSync] Sincronizado con servidor: %s\n", buf);
}

uint64_t now() {
    if (!_synced) {
        return millis();  // Fallback a millis local
    }
    // Tiempo actual = tiempo servidor + (millis actual - millis en sync)
    return _serverTimeAtSync + (millis() - _localMillisAtSync);
}

void formatTime(char* buffer) {
    uint64_t ms = now();
    
    if (!_synced) {
        // Sin sincronización: mostrar solo millis en segundos
        uint32_t secs = ms / 1000;
        uint16_t millis_part = ms % 1000;
        sprintf(buffer, "%lu.%03u", secs, millis_part);
        return;
    }
    
    // Convertir a hora del día (asumiendo UTC)
    uint32_t totalSecs = (ms / 1000) % 86400;  // Segundos en el día
    uint8_t hours = totalSecs / 3600;
    uint8_t mins = (totalSecs % 3600) / 60;
    uint8_t secs = totalSecs % 60;
    uint16_t millis_part = ms % 1000;
    
    sprintf(buffer, "%02d:%02d:%02d.%03d", hours, mins, secs, millis_part);
}

bool isSynced() {
    return _synced;
}

}  // namespace TimeSync
