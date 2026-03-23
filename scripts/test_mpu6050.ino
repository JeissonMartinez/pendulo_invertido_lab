/*
 * ============================================================
 *  TEST: Sensor MPU6050 (GY-521) sobre Carro Móvil
 *  Proyecto Péndulo Invertido — Fase 3
 * ============================================================
 *
 *  Objetivo:
 *    Verificar funcionamiento del MPU6050 en I2C Bus 0,
 *    leer aceleración (3 ejes), giroscopio (3 ejes),
 *    calcular pitch y roll con filtro complementario,
 *    y registrar datos mientras el carro se mueve por la pista.
 *
 *  Hardware:
 *    - ESP32-WROOM-32 DevKitC
 *    - MPU6050 (GY-521) montado sobre el carro móvil
 *    - Motor DC + BTS7960 H-Bridge
 *
 *  Conexión MPU6050:
 *    VCC → 3V3
 *    GND → GND
 *    SCL → GPIO 22 (I2C Bus 0, compartido con AS5600 #1)
 *    SDA → GPIO 21 (I2C Bus 0, compartido con AS5600 #1)
 *    AD0 → GND (dirección = 0x68)
 *
 *  Conexión Motor BTS7960:
 *    R_EN  → GPIO 26
 *    L_EN  → GPIO 27
 *    R_PWM → GPIO 14 (LEDC CH0, 25 kHz 10-bit)
 *    L_PWM → GPIO 12 (LEDC CH1, 25 kHz 10-bit)
 *
 *  Distribución I2C (nueva, post Fase 3):
 *    Bus 0 (GPIO 21/22): AS5600 #1 (0x36) + MPU6050 (0x68)
 *    Bus 1 (GPIO 18/19): AS5600 #2 (0x36) + OLED (0x3C)
 *
 *  Secuencia de prueba:
 *    1. Reposo 3 s (establishment baseline)
 *    2. Motor adelante PWM=300 por 5 s
 *    3. Parar 2 s
 *    4. Motor atrás PWM=300 por 5 s
 *    5. Parar (fin de prueba)
 *
 *  Salida Serial CSV (115200 baud, cada 50 ms):
 *    tiempo_ms, ax, ay, az, gx, gy, gz, pitch, roll
 *
 *  Nota: Este script usa lectura raw + filtro complementario.
 *        No usa el DMP del MPU6050 para mantener simplicidad.
 *
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>

// ============================================================
// I2C Bus 0 — MPU6050
// ============================================================
#define SDA_PIN  21
#define SCL_PIN  22
#define MPU6050_ADDR 0x68

// ============================================================
// Pines — Motor BTS7960
// ============================================================
#define PIN_R_EN   26
#define PIN_L_EN   27
#define PIN_R_PWM  14
#define PIN_L_PWM  12

// ============================================================
// Configuración PWM (LEDC)
// ============================================================
#define PWM_FREQ        25000
#define PWM_RESOLUTION  10
#define LEDC_CH_R       0
#define LEDC_CH_L       1

// ============================================================
// Tiempos de prueba (ms)
// ============================================================
#define PHASE_REST1      3000   // Reposo inicial
#define PHASE_FWD        5000   // Adelante
#define PHASE_REST2      2000   // Pausa
#define PHASE_REV        5000   // Atrás
// Después de REV, motor se detiene y se sigue imprimiendo

#define MOTOR_PWM_TEST   300    // PWM para la prueba
#define MPU_SAMPLE_MS    50     // Muestreo MPU6050 (20 Hz)

// ============================================================
// Constantes MPU6050
// ============================================================
#define ACCEL_SCALE  16384.0f   // LSB/g para ±2g
#define GYRO_SCALE   131.0f     // LSB/(°/s) para ±250°/s
#define RAD_TO_DEG   57.2957795f

// ============================================================
// Filtro complementario
// ============================================================
#define ALPHA  0.98f  // Peso del giroscopio

// ============================================================
// Variables
// ============================================================
float ax, ay, az;         // Aceleración en g
float gx, gy, gz;         // Velocidad angular en °/s
float pitch = 0.0f;       // Ángulo pitch (filtro complementario)
float roll  = 0.0f;       // Ángulo roll (filtro complementario)

unsigned long lastSampleTime = 0;
unsigned long testStartTime  = 0;
bool testFinished = false;

// Fases de la prueba
enum TestPhase {
    PH_REST1,
    PH_FWD,
    PH_REST2,
    PH_REV,
    PH_DONE
};
TestPhase currentPhase = PH_REST1;

// ============================================================
// Funciones del motor (BTS7960)
// ============================================================
void motorAdelante(int pwm) {
    ledcWrite(LEDC_CH_L, 0);
    ledcWrite(LEDC_CH_R, pwm);
}

void motorAtras(int pwm) {
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, pwm);
}

void motorFreno() {
    ledcWrite(LEDC_CH_R, 0);
    ledcWrite(LEDC_CH_L, 0);
}

// ============================================================
// Inicializar MPU6050
// ============================================================
bool initMPU6050() {
    // Despertar MPU6050 (PWR_MGMT_1, clock = PLL con X gyro)
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x6B);  // PWR_MGMT_1
    Wire.write(0x01);  // Clock PLL X gyro, no sleep
    if (Wire.endTransmission(true) != 0) return false;

    // Configurar acelerómetro ±2g
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x1C);  // ACCEL_CONFIG
    Wire.write(0x00);  // ±2g
    Wire.endTransmission(true);

    // Configurar giroscopio ±250°/s
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x1B);  // GYRO_CONFIG
    Wire.write(0x00);  // ±250°/s
    Wire.endTransmission(true);

    // Configurar DLPF (Digital Low Pass Filter) a ~44 Hz
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x1A);  // CONFIG
    Wire.write(0x03);  // DLPF_CFG = 3 → ~44 Hz accel, ~42 Hz gyro
    Wire.endTransmission(true);

    return true;
}

// ============================================================
// Leer 6 ejes del MPU6050
// ============================================================
bool readMPU6050() {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x3B);  // Registro ACCEL_XOUT_H
    if (Wire.endTransmission(false) != 0) return false;

    Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)14, (uint8_t)true);
    if (Wire.available() < 14) return false;

    // Acelerómetro (raw)
    int16_t ax_raw = (Wire.read() << 8) | Wire.read();
    int16_t ay_raw = (Wire.read() << 8) | Wire.read();
    int16_t az_raw = (Wire.read() << 8) | Wire.read();

    // Temperatura (raw) — leemos pero no usamos
    Wire.read(); Wire.read();

    // Giroscopio (raw)
    int16_t gx_raw = (Wire.read() << 8) | Wire.read();
    int16_t gy_raw = (Wire.read() << 8) | Wire.read();
    int16_t gz_raw = (Wire.read() << 8) | Wire.read();

    // Convertir a unidades físicas
    ax = ax_raw / ACCEL_SCALE;  // g
    ay = ay_raw / ACCEL_SCALE;
    az = az_raw / ACCEL_SCALE;
    gx = gx_raw / GYRO_SCALE;   // °/s
    gy = gy_raw / GYRO_SCALE;
    gz = gz_raw / GYRO_SCALE;

    return true;
}

// ============================================================
// Calcular pitch y roll con filtro complementario
// ============================================================
void updateOrientation(float dt) {
    // Ángulos del acelerómetro (solo válidos si no hay aceleración lineal)
    float accelPitch = atan2(ax, sqrt(ay * ay + az * az)) * RAD_TO_DEG;
    float accelRoll  = atan2(ay, sqrt(ax * ax + az * az)) * RAD_TO_DEG;

    // Filtro complementario
    pitch = ALPHA * (pitch + gy * dt) + (1.0f - ALPHA) * accelPitch;
    roll  = ALPHA * (roll  + gx * dt) + (1.0f - ALPHA) * accelRoll;
}

// ============================================================
// Nombre de la fase actual
// ============================================================
const char* getPhaseName() {
    switch (currentPhase) {
        case PH_REST1: return "REPOSO_1";
        case PH_FWD:   return "ADELANTE";
        case PH_REST2: return "REPOSO_2";
        case PH_REV:   return "ATRAS";
        case PH_DONE:  return "FIN";
        default:       return "???";
    }
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1500);

    // I2C Bus 0
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);  // 400 kHz fast mode

    // Motor — Enable
    pinMode(PIN_R_EN, OUTPUT);
    pinMode(PIN_L_EN, OUTPUT);
    digitalWrite(PIN_R_EN, HIGH);
    digitalWrite(PIN_L_EN, HIGH);

    // Motor — PWM (LEDC)
    ledcSetup(LEDC_CH_R, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(LEDC_CH_L, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_R_PWM, LEDC_CH_R);
    ledcAttachPin(PIN_L_PWM, LEDC_CH_L);
    motorFreno();

    // MPU6050
    Serial.println("=============================================================");
    Serial.println("  TEST: MPU6050 (GY-521) sobre Carro Movil");
    Serial.println("  I2C Bus 0 (GPIO 21/22) | Dir: 0x68 | +-2g / +-250 deg/s");
    Serial.println("  Motor BTS7960 | PWM prueba: 300 (10-bit)");
    Serial.println("=============================================================");
    Serial.println();

    if (!initMPU6050()) {
        Serial.println("ERROR: No se pudo inicializar MPU6050!");
        Serial.println("Verifica conexiones: SDA→GPIO21, SCL→GPIO22, AD0→GND");
        while (true) { delay(1000); }
    }

    Serial.println("MPU6050 inicializado correctamente.");
    Serial.println();

    // Encabezado CSV
    Serial.println("fase, tiempo_ms, ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, pitch_deg, roll_deg");

    // Inicializar tiempos
    testStartTime  = millis();
    lastSampleTime = millis();
    currentPhase   = PH_REST1;
}

// ============================================================
// Loop
// ============================================================
void loop() {
    unsigned long now = millis();
    unsigned long elapsed = now - testStartTime;

    // --- Gestión de fases ---
    if (!testFinished) {
        unsigned long totalRest1 = PHASE_REST1;
        unsigned long totalFwd   = totalRest1 + PHASE_FWD;
        unsigned long totalRest2 = totalFwd   + PHASE_REST2;
        unsigned long totalRev   = totalRest2 + PHASE_REV;

        TestPhase newPhase = currentPhase;

        if (elapsed < totalRest1) {
            newPhase = PH_REST1;
        } else if (elapsed < totalFwd) {
            newPhase = PH_FWD;
        } else if (elapsed < totalRest2) {
            newPhase = PH_REST2;
        } else if (elapsed < totalRev) {
            newPhase = PH_REV;
        } else {
            newPhase = PH_DONE;
        }

        // Transición de fase
        if (newPhase != currentPhase) {
            currentPhase = newPhase;
            switch (currentPhase) {
                case PH_REST1: motorFreno();                   break;
                case PH_FWD:   motorAdelante(MOTOR_PWM_TEST);  break;
                case PH_REST2: motorFreno();                   break;
                case PH_REV:   motorAtras(MOTOR_PWM_TEST);     break;
                case PH_DONE:
                    motorFreno();
                    testFinished = true;
                    Serial.println();
                    Serial.println("=== PRUEBA FINALIZADA ===");
                    Serial.println("Datos impresos en formato CSV.");
                    Serial.println("Copiar y pegar en archivo .csv para graficar.");
                    break;
            }
        }
    }

    // --- Muestreo MPU6050 ---
    if ((now - lastSampleTime) >= MPU_SAMPLE_MS && !testFinished) {
        float dt = (now - lastSampleTime) / 1000.0f;
        lastSampleTime = now;

        if (readMPU6050()) {
            updateOrientation(dt);

            // Imprimir CSV
            Serial.printf("%s, %lu, %.4f, %.4f, %.4f, %.2f, %.2f, %.2f, %.2f, %.2f\n",
                          getPhaseName(),
                          elapsed,
                          ax, ay, az,
                          gx, gy, gz,
                          pitch, roll);
        }
    }
}
