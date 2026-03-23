#include <Arduino.h>
#include "config.h"
#include "motor.h"
#include "encoder.h"
#include "state_machine.h"

static unsigned long lastPrintTime = 0;

void printHeader() {
    Serial.println("=============================================================");
    Serial.println("  Motor TB6612FNG + Encoder HC-020K — Test de Estados");
    Serial.println("  ESP32-S3-N16R8 | PPR: 20 | PWM: 10kHz 8-bit");
    Serial.println("=============================================================");
    Serial.println();
    Serial.printf("%-12s | %-4s | %-8s | %-7s | %-8s | %-6s | %-8s\n",
                  "Estado", "PWM", "Direccion", "Pulsos", "Vueltas", "RPS", "RPM");
    Serial.println("--------------------------------------------------------------");
}

void printStatus() {
    Serial.printf("%-12s | %3d  | %-8s  | %5lu   | %6.2f  | %5.2f | %7.2f\n",
                  sm_getStateName(),
                  motor_getSpeed(),
                  motor_getDirectionStr(),
                  (unsigned long)encoder_getPulseCount(),
                  encoder_getTurns(),
                  encoder_getRPS(),
                  encoder_getRPM());
}

void setup() {
    Serial.begin(115200);
    delay(1000);  // Esperar estabilización del serial

    printHeader();

    motor_init();
    encoder_init();
    sm_init();

    lastPrintTime = millis();
}

void loop() {
    sm_update();
    encoder_update();

    if (millis() - lastPrintTime >= SERIAL_PRINT_MS) {
        printStatus();
        lastPrintTime = millis();
    }
}