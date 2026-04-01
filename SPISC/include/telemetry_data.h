/**
 * telemetry_data.h — Estructuras de datos de telemetría unificada.
 * Contiene todos los campos de sensores del sistema.
 * 
 * Arquitectura Dual-Core:
 * - Los datos de telemetría se comparten entre Core 0 (sensores) y Core 1 (comunicación)
 * - Se protegen con mutex para acceso thread-safe
 */
#ifndef TELEMETRY_DATA_H
#define TELEMETRY_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ══════════════════════════════════════════════════
// Dirección del motor
// ══════════════════════════════════════════════════
enum MotorDir {
    DIR_STOP     =  0,
    DIR_ADELANTE =  1,
    DIR_ATRAS    = -1
};

// ══════════════════════════════════════════════════
// Modos de operación del firmware
// ══════════════════════════════════════════════════
enum FirmwareMode {
    MODE_IDLE,
    MODE_SWEEP_PWM,
    MODE_OPEN_LOOP,
    MODE_MANUAL,
    MODE_PID,
    MODE_PID_POS,
    MODE_NEURAL,
    MODE_SMC,
    MODE_LQR,
    MODE_FUZZY,
    MODE_MRAC,
    MODE_MPC
};

// ══════════════════════════════════════════════════
// Paquete de telemetría completo (todos los sensores)
// ══════════════════════════════════════════════════
struct TelemetryPacket {
    // Encoder IR (HC-020K)
    uint32_t pulsos_ir;
    float    vueltas_ir;
    float    rps_ir;         // revoluciones por segundo
    float    rpm_ir;         // revoluciones por minuto
    float    pos_ir_cm;      // posición en cm

    // Encoder AS5600 motor (posición carro)
    float    rps_mag;        // revoluciones por segundo (magnético)
    float    rpm_mag;        // revoluciones por minuto (magnético)
    float    pos_mag_cm;     // posición en cm (magnético)

    // Encoder AS5600 péndulo (ángulo)
    float    angle_pend;     // ángulo del péndulo en grados
    float    vel_ang_pend;   // velocidad angular (°/s)

    // MPU6050
    float    mpu_ax;         // aceleración X (m/s²)
    float    mpu_ay;         // aceleración Y
    float    mpu_az;         // aceleración Z
    float    mpu_gx;         // giroscopio X (°/s)
    float    mpu_gy;         // giroscopio Y
    float    mpu_gz;         // giroscopio Z
    float    mpu_pitch;      // pitch calculado (°)
    float    mpu_roll;       // roll calculado (°)

    // Motor
    int      pwm;
    MotorDir direction;

    // Limit switches
    bool     ls_start;
    bool     ls_end;

    // Timestamp y modo
    unsigned long ts;
    FirmwareMode  modo;
    const char*   ol_state;  // Sub-estado del modo activo (e.g. "OL_READY")

    // Control PID Posición
    float pid_error;         // Error actual: setpoint - pos_actual (cm)
    float pid_integral;      // Término integral acumulado
    float pid_derivada;      // Término derivativo (filtrado)
    float pid_output;        // Salida del controlador (PWM)
    float pid_setpoint;      // Setpoint configurado (cm)
    float pid_kp;            // Constante proporcional
    float pid_ki;            // Constante integral
    float pid_kd;            // Constante derivativa
    float pid_zona_seg;      // Zona de seguridad (cm)
    float pid_tolerancia;    // Tolerancia de llegada (cm)
};

// Helpers para convertir enums a string
inline const char* motorDirToStr(MotorDir d) {
    switch (d) {
        case DIR_ADELANTE: return "ADELANTE";
        case DIR_ATRAS:    return "ATRAS";
        default:           return "DETENIDO";
    }
}

inline const char* firmwareModeToStr(FirmwareMode m) {
    switch (m) {
        case MODE_SWEEP_PWM:  return "SWEEP_PWM";
        case MODE_OPEN_LOOP:  return "OPEN_LOOP";
        case MODE_MANUAL:     return "MANUAL";
        case MODE_PID:        return "PID";
        case MODE_PID_POS:    return "PID_POS";
        case MODE_NEURAL:     return "NEURAL";
        case MODE_SMC:        return "SMC";
        case MODE_LQR:        return "LQR";
        case MODE_FUZZY:      return "FUZZY";
        case MODE_MRAC:       return "MRAC";
        case MODE_MPC:        return "MPC";
        default:              return "IDLE";
    }
}

inline FirmwareMode strToFirmwareMode(const char* s) {
    if (strcmp(s, "SWEEP_PWM")  == 0) return MODE_SWEEP_PWM;
    if (strcmp(s, "OPEN_LOOP")  == 0) return MODE_OPEN_LOOP;
    if (strcmp(s, "MANUAL")     == 0) return MODE_MANUAL;
    if (strcmp(s, "PID")        == 0) return MODE_PID;
    if (strcmp(s, "PID_POS")    == 0) return MODE_PID_POS;
    if (strcmp(s, "NEURAL")     == 0) return MODE_NEURAL;
    if (strcmp(s, "SMC")        == 0) return MODE_SMC;
    if (strcmp(s, "LQR")        == 0) return MODE_LQR;
    if (strcmp(s, "FUZZY")      == 0) return MODE_FUZZY;
    if (strcmp(s, "MRAC")       == 0) return MODE_MRAC;
    if (strcmp(s, "MPC")        == 0) return MODE_MPC;
    return MODE_IDLE;
}

// ══════════════════════════════════════════════════
// Thread-safety para datos compartidos (Dual-Core)
// ══════════════════════════════════════════════════

/** Mutex global para acceso a telemetría compartida */
extern SemaphoreHandle_t telemetryMutex;

/** Inicializar mutex de telemetría. Llamar antes de crear tareas. */
inline void initTelemetryMutex() {
    if (telemetryMutex == NULL) {
        telemetryMutex = xSemaphoreCreateMutex();
    }
}

/** 
 * Adquirir mutex para acceso seguro a telemetría.
 * @param waitMs Tiempo máximo de espera (ms). 0 = intentar sin bloquear.
 * @return true si se adquirió el mutex.
 */
inline bool lockTelemetry(uint32_t waitMs = portMAX_DELAY) {
    if (telemetryMutex == NULL) return false;
    return xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

/** Liberar mutex de telemetría. */
inline void unlockTelemetry() {
    if (telemetryMutex != NULL) {
        xSemaphoreGive(telemetryMutex);
    }
}

#endif // TELEMETRY_DATA_H
