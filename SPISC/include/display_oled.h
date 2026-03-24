/**
 * display_oled.h — OLED SSD1306 128×64 en I2C Bus 1.
 */
#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <Arduino.h>
#include "telemetry_data.h"

namespace DisplayOLED {

/** Inicializa la pantalla OLED en Bus 1. */
bool begin();

/** Actualiza la pantalla con los datos de telemetría. */
void update(const TelemetryPacket& pkt, const char* stateStr);

/** Muestra un mensaje simple centrado. */
void showMessage(const char* line1, const char* line2 = nullptr);

}  // namespace DisplayOLED

#endif // DISPLAY_OLED_H
