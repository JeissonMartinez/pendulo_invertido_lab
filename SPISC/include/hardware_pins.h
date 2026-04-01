/**
 * hardware_pins.h — Constantes de pines GPIO y hardware del péndulo invertido.
 * Centraliza toda la configuración de pines para evitar duplicación.
 */
#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// ══════════════════════════════════════════════════
// Motor BTS7960 H-Bridge
// ══════════════════════════════════════════════════
#define PIN_R_EN    26
#define PIN_L_EN    27
#define PIN_R_PWM   12
#define PIN_L_PWM   14

// ══════════════════════════════════════════════════
// PWM (LEDC)
// ══════════════════════════════════════════════════
#define PWM_FREQ        25000
#define PWM_RESOLUTION  10       // 0-1023
#define LEDC_CH_R       0
#define LEDC_CH_L       1

// ══════════════════════════════════════════════════
// Encoder Infrarrojo HC-020K (con PCNT)
// ══════════════════════════════════════════════════
#define PIN_ENCODER_IR     16
#define PULSES_PER_REV     40
#define CM_POR_PULSO       0.0992f  // Calibrado: 90cm / 907 pulsos (PCNT)

// PCNT (Pulse Counter) Hardware
#define PCNT_UNIT_ENCODER  PCNT_UNIT_0
#define PCNT_CHANNEL_ENC   PCNT_CHANNEL_0
#define PCNT_FILTER_VAL    100      // Filtro anti-glitch (~1.25µs @ 80MHz)
#define PCNT_H_LIM         10000    // Límite superior antes de overflow
#define PCNT_L_LIM         -10000   // Límite inferior antes de underflow

// ══════════════════════════════════════════════════
// Limit Switches XV-152-1C25 (NC + Pull-Up)
// ══════════════════════════════════════════════════
#define PIN_LIMIT_START  32
#define PIN_LIMIT_END    33
#define LS_DEBOUNCE_US   50000   // 50 ms debounce

// ══════════════════════════════════════════════════
// I2C Buses
// ══════════════════════════════════════════════════
// Bus 0: AS5600 #1 (ángulo péndulo) + MPU6050
#define I2C_BUS0_SDA     21
#define I2C_BUS0_SCL     22

// Bus 1: AS5600 #2 (posición carro) + OLED SSD1306
#define I2C_BUS1_SDA     18
#define I2C_BUS1_SCL     19

// ══════════════════════════════════════════════════
// I2C Direcciones
// ══════════════════════════════════════════════════
#define AS5600_ADDR      0x36
#define MPU6050_ADDR     0x68
#define OLED_ADDR        0x3C

// ══════════════════════════════════════════════════
// OLED SSD1306
// ══════════════════════════════════════════════════
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64

// ══════════════════════════════════════════════════
// LED indicador
// ══════════════════════════════════════════════════
#define PIN_LED          13

// ══════════════════════════════════════════════════
// Constantes de conversión y geometría de la pista
// ══════════════════════════════════════════════════
#define TRACK_LENGTH_CM  90.0f    // Longitud total de la pista (cm)
#define CM_POR_GRADO     0.01103f // Calibrado: 90cm / 8158° = 0.01103 cm/°

// ══════════════════════════════════════════════════
// Intervalos de publicación MQTT (ms)
// ══════════════════════════════════════════════════
#define TELEMETRY_INTERVAL_MS   200   // 5 Hz
#define STATUS_INTERVAL_MS      500   // 2 Hz

// ══════════════════════════════════════════════════
// PWM defaults
// ══════════════════════════════════════════════════
#define HOMING_PWM       300

#endif // HARDWARE_PINS_H
