#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Pines - Motor TB6612FNG (Canal B)
// ============================================================
#define PIN_STBY    9    // Standby (HIGH = activo, LOW = apagado)
#define PIN_BIN1    11   // Dirección 1
#define PIN_BIN2    12   // Dirección 2
#define PIN_PWMB    13   // PWM velocidad

// ============================================================
// Pines - Encoder HC-020K
// ============================================================
#define PIN_ENCODER 10   // Señal de pulsos

// ============================================================
// Configuración PWM (LEDC)
// ============================================================
#define PWM_FREQ       10000  // 10 kHz
#define PWM_RESOLUTION 8      // 8-bit (0-255)

// ============================================================
// Encoder
// ============================================================
#define PULSES_PER_REV 20     // 20 ranuras en la rueda

// ============================================================
// Niveles de velocidad PWM
// ============================================================
#define SPEED_LEVELS   5
const uint8_t SPEED_VALUES[SPEED_LEVELS] = {0, 85, 170, 212, 255};

// ============================================================
// Tiempos (ms)
// ============================================================
#define STATE_DURATION_MS   4000  // Duración de cada estado (4 s)
#define ENCODER_CALC_MS     1000  // Ventana de cálculo de velocidad (1 s)
#define SERIAL_PRINT_MS     500   // Intervalo de impresión serial (0.5 s)

#endif // CONFIG_H
