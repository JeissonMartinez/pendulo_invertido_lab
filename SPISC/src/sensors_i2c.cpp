/**
 * sensors_i2c.cpp — Implementación de lectura de sensores I2C.
 */
#include "sensors_i2c.h"
#include "hardware_pins.h"
#include <AS5600.h>

namespace SensorsI2C {

// ── Buses I2C (usar globales de Arduino para compartir con OLED) ─
// Wire  = Bus 0 (SDA=21, SCL=22): AS5600 péndulo + MPU6050
// Wire1 = Bus 1 (SDA=18, SCL=19): AS5600 carro + OLED SSD1306

// ── Sensores AS5600 ──────────────────────────
static AS5600 _as5600_pend(&Wire);    // Ángulo péndulo (Bus 0)
static AS5600 _as5600_cart(&Wire1);   // Posición carro (Bus 1)

// ── Variables AS5600 péndulo ─────────────────
static float _anglePend     = 0;
static float _angleOffset   = 0;
static float _velAngPend    = 0;
static float _prevAnglePend = 0;

// ── Variables AS5600 carro (magnético) ───────
static float  _posMagCm    = 0;
static float  _rpsMag      = 0;
static float  _rpmMag      = 0;
static long   _magVueltas  = 0;
static int32_t _lastMagRaw = 0;
static long   _posAcumulada = 0;

// ── Variables MPU6050 ────────────────────────
static float _ax = 0, _ay = 0, _az = 0;
static float _gx = 0, _gy = 0, _gz = 0;
static float _pitch = 0, _roll = 0;
static bool  _mpuOk = false;

// ── Temporización ────────────────────────────
static unsigned long _lastSensorRead = 0;
static unsigned long _lastMagCalc    = 0;
static const unsigned long SENSOR_PERIOD_US = 2000;  // 2 ms (500 Hz)

// ── MPU6050 raw read (sin librería externa, registro directo) ──
static void _mpuWriteReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static bool _mpuInit() {
    // Wake up MPU6050
    _mpuWriteReg(0x6B, 0x00);  // PWR_MGMT_1: salir de sleep
    delay(10);

    // Verificar WHO_AM_I
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x75);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)1);
    if (!Wire.available()) return false;
    uint8_t whoami = Wire.read();
    if (whoami != 0x68 && whoami != 0x98) return false;

    // Configurar: acelerómetro ±2g, giroscopio ±250°/s
    _mpuWriteReg(0x1C, 0x00);  // ACCEL_CONFIG: ±2g
    _mpuWriteReg(0x1B, 0x00);  // GYRO_CONFIG: ±250°/s
    _mpuWriteReg(0x1A, 0x03);  // DLPF_CFG: ~43 Hz bandwidth

    return true;
}

static void _mpuRead() {
    if (!_mpuOk) return;

    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x3B);  // ACCEL_XOUT_H
    if (Wire.endTransmission(false) != 0) return;
    Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)14);
    if (Wire.available() < 14) return;

    int16_t axRaw = (Wire.read() << 8) | Wire.read();
    int16_t ayRaw = (Wire.read() << 8) | Wire.read();
    int16_t azRaw = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read();  // temp (skip)
    int16_t gxRaw = (Wire.read() << 8) | Wire.read();
    int16_t gyRaw = (Wire.read() << 8) | Wire.read();
    int16_t gzRaw = (Wire.read() << 8) | Wire.read();

    // Conversión: ±2g → 16384 LSB/g, ±250°/s → 131 LSB/(°/s)
    _ax = axRaw / 16384.0f * 9.81f;
    _ay = ayRaw / 16384.0f * 9.81f;
    _az = azRaw / 16384.0f * 9.81f;
    _gx = gxRaw / 131.0f;
    _gy = gyRaw / 131.0f;
    _gz = gzRaw / 131.0f;

    // Filtro complementario para pitch y roll
    static unsigned long prevT = 0;
    unsigned long now = micros();
    float dt = (prevT > 0) ? (now - prevT) / 1000000.0f : 0.002f;
    prevT = now;

    float accelPitch = atan2(_ax, sqrt(_ay * _ay + _az * _az)) * 180.0f / PI;
    float accelRoll  = atan2(_ay, sqrt(_ax * _ax + _az * _az)) * 180.0f / PI;

    _pitch = 0.98f * (_pitch + _gy * dt) + 0.02f * accelPitch;
    _roll  = 0.98f * (_roll  + _gx * dt) + 0.02f * accelRoll;
}

// ── Inicialización ───────────────────────────
void begin() {
    // Inicializar buses I2C globales a 400kHz (Fast Mode)
    // AS5600, MPU6050 y SSD1306 soportan 400kHz.
    // Reduce bloqueo I2C de ~92ms a ~23ms por frame OLED.
    Wire.begin(I2C_BUS0_SDA, I2C_BUS0_SCL);    // Bus 0: AS5600 péndulo + MPU6050
    Wire.setClock(400000);
    Wire1.begin(I2C_BUS1_SDA, I2C_BUS1_SCL);   // Bus 1: AS5600 carro + OLED
    Wire1.setClock(400000);

    // AS5600 péndulo (Bus 0)
    _as5600_pend.resetPosition();

    // AS5600 carro (Bus 1)
    _as5600_cart.resetPosition();
    _as5600_cart.resetCumulativePosition();

    // MPU6050 (Bus 0)
    _mpuOk = _mpuInit();
    if (_mpuOk) {
        Serial.println("[sensors] MPU6050 inicializado OK");
    } else {
        Serial.println("[sensors] MPU6050 no detectado — continuando sin IMU");
    }

    _lastSensorRead = micros();
    _lastMagCalc    = millis();
}

// ── Actualización periódica ──────────────────
void update() {
    unsigned long now = micros();
    if ((now - _lastSensorRead) < SENSOR_PERIOD_US) return;
    _lastSensorRead = now;

    // ── AS5600 #1: Ángulo del péndulo ──────
    float rawAngle = map(_as5600_pend.readAngle(), 0, 4095, 3600, 0) / 10.0f;
    _anglePend = fmod(rawAngle - _angleOffset + 360.0f, 360.0f);

    // Velocidad angular del péndulo
    float dAngle = _anglePend - _prevAnglePend;
    if (dAngle > 180.0f) dAngle -= 360.0f;
    if (dAngle < -180.0f) dAngle += 360.0f;
    float dtSec = SENSOR_PERIOD_US / 1000000.0f;
    _velAngPend = dAngle / dtSec;
    _prevAnglePend = _anglePend;

    // ── AS5600 #2: Posición del carro ──────
    int32_t rawPos = _as5600_cart.getCumulativePosition();
    int32_t delta = rawPos - _lastMagRaw;

    if (delta < -2000) _magVueltas++;
    if (delta > 2000)  _magVueltas--;

    _lastMagRaw = rawPos;
    _posAcumulada = (_magVueltas * 360) + map(rawPos, 0, 4095, 0, 360);
    _posMagCm = _posAcumulada * CM_POR_GRADO;

    // RPM/RPS del encoder magnético (~cada 200ms)
    unsigned long nowMs = millis();
    if ((nowMs - _lastMagCalc) >= 200) {
        static float prevPosCm = 0;
        float dPos = _posMagCm - prevPosCm;
        float dtCalc = (nowMs - _lastMagCalc) / 1000.0f;
        if (dtCalc > 0) {
            // Convertir cm/s → rev/s (usando circunferencia de polea)
            // Simplificación: usar relación de grados/s
            float degPerSec = (dPos / CM_POR_GRADO) / dtCalc;
            _rpsMag = degPerSec / 360.0f;
            _rpmMag = _rpsMag * 60.0f;
        }
        prevPosCm = _posMagCm;
        _lastMagCalc = nowMs;
    }

    // ── MPU6050 ────────────────────────────
    _mpuRead();
}

// ── Getters AS5600 péndulo ───────────────────
float getAnglePend()      { return _anglePend; }
float getAngularVelPend() { return _velAngPend; }

void calibrateAngle() {
    _angleOffset = map(_as5600_pend.readAngle(), 0, 4095, 3600, 0) / 10.0f;
    Serial.printf("[sensors] Ángulo calibrado. Offset: %.1f°\n", _angleOffset);
}

float getAngleOffset() { return _angleOffset; }

// ── Getters AS5600 carro ─────────────────────
float getPosMagCm() { return _posMagCm; }
float getRpsMag()   { return _rpsMag; }
float getRpmMag()   { return _rpmMag; }

void resetMagPosition() {
    _as5600_cart.resetPosition();
    _as5600_cart.resetCumulativePosition();
    _magVueltas   = 0;
    _lastMagRaw   = 0;
    _posAcumulada = 0;
    _posMagCm     = 0;
}

// ── Getters MPU6050 ──────────────────────────
float getAccelX() { return _ax; }
float getAccelY() { return _ay; }
float getAccelZ() { return _az; }
float getGyroX()  { return _gx; }
float getGyroY()  { return _gy; }
float getGyroZ()  { return _gz; }
float getPitch()  { return _pitch; }
float getRoll()   { return _roll; }

}  // namespace SensorsI2C
