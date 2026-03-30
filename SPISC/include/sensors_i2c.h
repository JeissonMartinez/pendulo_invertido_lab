/**
 * sensors_i2c.h — Lectura de sensores I2C:
 *   - AS5600 #1 (ángulo péndulo, Bus 0)
 *   - AS5600 #2 (posición carro magnética, Bus 1)
 *   - MPU6050   (IMU 6 ejes, Bus 0)
 */
#ifndef SENSORS_I2C_H
#define SENSORS_I2C_H

#include <Arduino.h>
#include <Wire.h>

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

}  // namespace SensorsI2C

#endif // SENSORS_I2C_H
