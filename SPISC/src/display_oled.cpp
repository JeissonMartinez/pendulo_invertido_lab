/**
 * display_oled.cpp — Implementación OLED SSD1306 en Bus 1.
 * 
 * Nota: La OLED se movió al Bus 1 (GPIO 18/19) para dejar
 *       espacio al MPU6050 en el Bus 0 (GPIO 21/22).
 * 
 * Arquitectura Dual-Core:
 *   - Wire1 (Bus 1) es compartido entre sensores (Core 0) y OLED (Core 1)
 *   - Se usa i2cBus1Mutex de SensorsI2C para acceso thread-safe
 */
#include "display_oled.h"
#include "hardware_pins.h"
#include "sensors_i2c.h"  // Para mutex I2C
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace DisplayOLED {

// Usar Bus 1 para la OLED (compartido con AS5600 #2 de posición)
static TwoWire& _wire = Wire1;
static Adafruit_SSD1306* _display = nullptr;

bool begin() {
    // Wire1 ya debe estar inicializado por SensorsI2C::begin()
    // Adquirir mutex para inicialización
    if (!SensorsI2C::lockI2CBus1(100)) {
        Serial.println("[oled] ERROR: No se pudo adquirir mutex I2C para init");
        return false;
    }
    
    _display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &_wire);
    bool ok = _display->begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    
    if (!ok) {
        Serial.println("[oled] SSD1306 no detectado en Bus 1");
        delete _display;
        _display = nullptr;
        SensorsI2C::unlockI2CBus1();
        return false;
    }
    
    _display->clearDisplay();
    _display->setTextColor(SSD1306_WHITE);
    _display->display();
    
    SensorsI2C::unlockI2CBus1();
    Serial.println("[oled] OLED inicializado en Bus 1");
    return true;
}

void update(const TelemetryPacket& pkt, const char* stateStr) {
    if (!_display) return;

    // Adquirir mutex antes de acceder a Wire1
    if (!SensorsI2C::lockI2CBus1(30)) {
        // No se pudo adquirir mutex, omitir esta actualización
        return;
    }

    _display->clearDisplay();
    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);

    // Línea 1: Ángulo
    _display->setCursor(0, 0);
    _display->print("Ang:");
    _display->setCursor(0, 10);
    _display->setTextSize(2);
    _display->print(pkt.angle_pend, 1);
    _display->print((char)247);  // ° symbol

    // Estado (arriba derecha)
    _display->setTextSize(1);
    _display->setCursor(80, 0);
    _display->print(stateStr);

    // Línea 2: Posición
    _display->setCursor(0, 28);
    _display->print("Pos:");
    _display->setCursor(0, 38);
    _display->setTextSize(2);
    _display->print(pkt.pos_mag_cm, 1);

    // PWM y dirección (derecha)
    _display->setTextSize(1);
    _display->setCursor(80, 15);
    _display->print("PWM:");
    _display->print(pkt.pwm);

    _display->setCursor(80, 28);
    _display->print("Dir:");
    _display->print(motorDirToStr(pkt.direction));

    // Modo (abajo)
    _display->setCursor(80, 40);
    _display->print(firmwareModeToStr(pkt.modo));

    // Limit switches
    _display->setCursor(0, 55);
    _display->print("LS:");
    _display->print(pkt.ls_start ? "S" : ".");
    _display->print(pkt.ls_end   ? "E" : ".");

    // RPM
    _display->setCursor(40, 55);
    _display->print("RPM:");
    _display->print(pkt.rpm_ir, 0);

    _display->display();
    
    SensorsI2C::unlockI2CBus1();  // Liberar mutex
}

void showMessage(const char* line1, const char* line2) {
    if (!_display) return;
    
    // Adquirir mutex antes de acceder a Wire1
    if (!SensorsI2C::lockI2CBus1(50)) {
        return;  // Omitir si no se puede adquirir
    }
    
    _display->clearDisplay();
    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);
    _display->setCursor(0, 20);
    _display->print(line1);
    if (line2) {
        _display->setCursor(0, 36);
        _display->print(line2);
    }
    _display->display();
    
    SensorsI2C::unlockI2CBus1();
}

}  // namespace DisplayOLED
