/**
 * sensors_i2c.h — Lectura de sensores I2C:
 *   - AS5600 #1 (ángulo péndulo, Bus 0)
 *   - AS5600 #2 (posición carro magnética, Bus 1)
 *   - MPU6050   (IMU 6 ejes, Bus 0)
 * 
 * Arquitectura Dual-Core:
 *   - Wire1 (Bus 1) es compartido entre sensores (Core 0) y OLED (Core 1)
 *   - Se usa un mutex para acceso thread-safe
 */
#ifndef SENSORS_I2C_H
#define SENSORS_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace SensorsI2C {

/** Inicializa los buses I2C y todos los sensores. */
void begin();

/**
 * Lee todos los sensores. Llamar frecuentemente (~500 Hz).
 * Internamente usa un cronómetro de ~2 ms para AS5600.
 */
void update();

// ── AS5600 #1: Ángulo del péndulo ────────────
float getAnglePend();         // grados (0-360, 0=arriba, 180=abajo)
float getAngularVelPend();    // °/s

/** Calibrar offset de ángulo (péndulo colgando → 180°). */
void calibrateAngle();
float getAngleOffset();
bool isAngleCalibrated();

// ── AS5600 #2: Posición del carro (magnético) ─
float getPosMagCm();          // posición en cm
float getRpsMag();            // rev/s del AS5600 motor
float getRpmMag();            // rev/min del AS5600 motor

/** Resetear posición magnética a cero. */
void resetMagPosition();
bool isMagPosCalibrated();

// ── MPU6050: IMU ─────────────────────────────
float getAccelX();            // m/s²
float getAccelY();
float getAccelZ();
float getGyroX();             // °/s
float getGyroY();
float getGyroZ();
float getPitch();             // grados (filtro complementario)
float getRoll();              // grados (filtro complementario)

/** Calibrar offsets del giroscopio (MPU debe estar en reposo). */
void calibrateMPU();
bool isMPUCalibrated();

// ══════════════════════════════════════════════════
// Mutex I2C para acceso thread-safe (Dual-Core)
// ══════════════════════════════════════════════════

/** Mutex para Bus 1 (Wire1) - compartido con OLED */
extern SemaphoreHandle_t i2cBus1Mutex;

/** Inicializar mutex I2C. Llamar en setup() antes de crear tareas. */
void initI2CMutex();

/** 
 * Adquirir mutex para Wire1.
 * @param waitMs Tiempo máximo de espera (ms). 0 = intentar sin bloquear.
 * @return true si se adquirió el mutex.
 */
bool lockI2CBus1(uint32_t waitMs = 50);

/** Liberar mutex de Wire1. */
void unlockI2CBus1();

}  // namespace SensorsI2C

#endif // SENSORS_I2C_H
