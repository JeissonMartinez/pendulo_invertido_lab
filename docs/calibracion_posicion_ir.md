# Calibración del Encoder Infrarrojo HC-020K (Posición del Carro)

**Proyecto:** [SPISC — Sistema de Péndulo Invertido con Supervisión y Control](../README.md)  
**Fecha:** 31/03/2026  
**Autor:** Ing. Jeisson Martínez Flórez

---

## Tabla de contenidos

- [Descripción general](#descripción-general)
- [Conexión del hardware](#conexión-del-hardware)
- [Configuración actual](#configuración-actual)
- [Funciones implementadas](#funciones-implementadas)
- [Problemas detectados](#problemas-detectados)
- [Análisis técnico del problema](#análisis-técnico-del-problema)
- [Mejoras propuestas](#mejoras-propuestas)
- [Plan de implementación](#plan-de-implementación)
- [Pruebas de validación](#pruebas-de-validación)

---

## Descripción general

El **HC-020K** es un sensor encoder infrarrojo de tipo optoacoplador que utiliza un LED IR emisor y un fototransistor receptor, junto con un comparador LM393 para generar pulsos digitales limpios. Se utiliza para medir la posición lineal del carro sobre el riel mediante una rueda ranurada acoplada al sistema motor-polea-correa.

### Principio de funcionamiento

```
┌─────────────────────────────────────────────────────────────┐
│  Motor → Polea → Correa → Carro                            │
│              ↓                                              │
│         Rueda ranurada (40 rendijas)                       │
│              ↓                                              │
│         HC-020K (LED IR + Fototransistor + LM393)          │
│              ↓                                              │
│         Pulsos digitales → ESP32 GPIO 16                   │
│              ↓                                              │
│         ISR cuenta pulsos → Posición en cm                 │
└─────────────────────────────────────────────────────────────┘
```

### Características del sensor

| Parámetro | Valor |
|-----------|-------|
| **Modelo** | HC-020K |
| **Tipo** | Optoacoplador con comparador LM393 |
| **Voltaje operación** | 3.3V - 5V |
| **Salida** | Digital (TTL) |
| **Frecuencia máxima típica** | 3 - 5 kHz |
| **Ancho de ranura óptimo** | 5 - 10 mm |

---

## Conexión del hardware

### Pinout del sensor

| Pin HC-020K | Conexión ESP32 | Descripción |
|-------------|----------------|-------------|
| VCC | 3.3V | Alimentación |
| GND | GND | Tierra |
| OUT | GPIO 16 | Señal de pulsos |

### Rueda ranurada

| Parámetro | Valor |
|-----------|-------|
| **Número de ranuras** | 40 |
| **Pulsos por revolución** | 40 PPR |
| **Acoplamiento** | Eje motor/polea |

### Calibración mecánica

La constante `CM_POR_PULSO` se determinó experimentalmente:

```
Distancia recorrida = 90 cm (longitud útil del riel)
Pulsos medidos = ~1786 pulsos
CM_POR_PULSO = 90 / 1786 ≈ 0.0504 cm/pulso
```

---

## Configuración actual

### Constantes de hardware

Archivo: [`SPISC/include/hardware_pins.h`](../SPISC/include/hardware_pins.h)

```cpp
// Encoder Infrarrojo HC-020K
#define PIN_ENCODER_IR   16
#define PULSES_PER_REV   40
#define CM_POR_PULSO     0.0504f
```

### Implementación del driver

Archivo: [`SPISC/src/encoder_ir.cpp`](../SPISC/src/encoder_ir.cpp)

```cpp
// Variables ISR (volatile)
static volatile uint32_t _isrPulseCount = 0;
static volatile int32_t  _globalPosition = 0;

// ISR del encoder (método actual)
static void IRAM_ATTR _encoderISR() {
    _isrPulseCount++;
    MotorDir dir = Motor::getDirection();
    if (dir == DIR_ADELANTE)
        _globalPosition++;
    else if (dir == DIR_ATRAS)
        _globalPosition--;
}

void begin() {
    pinMode(PIN_ENCODER_IR, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_IR), _encoderISR, RISING);
    _prevTime = millis();
}
```

### Fórmula de conversión

```cpp
_posCm = getGlobalPosition() * CM_POR_PULSO;
// Ejemplo: 500 pulsos × 0.0504 = 25.2 cm
```

---

## Funciones implementadas

### Namespace: `EncoderIR`

Archivo: [`SPISC/include/encoder_ir.h`](../SPISC/include/encoder_ir.h)

| Función | Descripción |
|---------|-------------|
| `begin()` | Inicializa GPIO y configura interrupción en `RISING` |
| `getPulseCount()` | Retorna conteo de pulsos (atómico con `noInterrupts()`) |
| `resetPulseCount()` | Resetea contador de pulsos a cero |
| `getGlobalPosition()` | Retorna posición con signo (considera dirección) |
| `resetGlobalPosition()` | Resetea posición global a cero |
| `updateRPM()` | Calcula RPM/RPS basado en delta de pulsos |
| `getRPS()` / `getRPM()` | Retorna velocidad angular calculada |
| `getPositionCm()` | Retorna posición lineal en centímetros |

### Datos publicados en telemetría

```json
{
  "pulsos_ir": 1234,
  "vueltas_ir": 30.85,
  "rps_ir": 5.2,
  "rpm_ir": 312.0,
  "pos_ir_cm": 62.19
}
```

---

## Problemas detectados

### Síntoma observado

Al aplicar PWM alto (cercano al máximo) al motor, la lectura de posición del encoder IR **pierde pulsos** y reporta una distancia menor a la real. El problema es más evidente en:

- Arranques bruscos con PWM > 800/1023
- Movimientos rápidos sostenidos
- Cambios de dirección a alta velocidad

### Evidencia del problema

```
Esperado:  Carro recorre 40 cm → ~794 pulsos
Medido:    Carro recorre 40 cm → ~650-700 pulsos (pérdida del 12-18%)
```

### Impacto en el sistema

1. **Control de posición inexacto** — El péndulo invertido requiere conocer la posición precisa del carro para el balance.
2. **Límites de seguridad imprecisos** — Riesgo de golpear los topes físicos del riel.
3. **Caracterización incorrecta** — Los experimentos de rango PWM reportan RPM menores a los reales.

---

## Análisis técnico del problema

### Cálculo de frecuencia de pulsos

Con los parámetros del sistema:

| Escenario | Velocidad carro | Frecuencia pulsos |
|-----------|-----------------|-------------------|
| PWM bajo (~30%) | ~20 cm/s | ~400 Hz |
| PWM medio (~50%) | ~50 cm/s | ~1000 Hz |
| PWM alto (~80%) | ~100 cm/s | ~2000 Hz |
| PWM máximo (~100%) | ~130 cm/s | **~2600 Hz** |

### Causas probables de pérdida de pulsos

| Causa | Descripción | Probabilidad |
|-------|-------------|--------------|
| **1. Rebote del optoacoplador** | Transiciones múltiples en el flanco debido a ruido óptico o mecánico | Alta |
| **2. Tiempo de respuesta del LM393** | El comparador no estabiliza la salida antes del siguiente pulso | Alta |
| **3. Latencia de ISR** | Otras interrupciones (WiFi, I2C) bloquean la ISR del encoder | Media |
| **4. Ruido eléctrico del motor** | EMI del motor DC induce pulsos espurios o enmascara pulsos reales | Media |
| **5. Rendijas muy angostas** | La señal IR no tiene tiempo de estabilizar entre ranuras | Media |

### Limitaciones de la implementación actual

```cpp
// Problema 1: attachInterrupt usa interrupciones de software
attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_IR), _encoderISR, RISING);
// ↳ Puede perder pulsos si otra ISR está ejecutándose

// Problema 2: noInterrupts() afecta todo el sistema
uint32_t getPulseCount() {
    noInterrupts();        // Bloquea TODAS las interrupciones
    uint32_t c = _isrPulseCount;
    interrupts();
    return c;
}

// Problema 3: Solo cuenta flancos RISING (40 PPR efectivos)
// Podría usar CHANGE para duplicar resolución (80 PPR efectivos)
```

---

## Mejoras propuestas

### Opción A: Contador PCNT por hardware (Recomendada)

El ESP32 incluye el periférico **PCNT (Pulse Counter)** que cuenta pulsos por hardware sin intervención de la CPU.

#### Ventajas

- Conteo por hardware hasta ~40 MHz (muy superior a los ~5 kHz del HC-020K)
- Filtro anti-glitch configurable (elimina rebotes de ns a µs)
- No consume tiempo de CPU
- Inmune a latencia de otras interrupciones
- Soporta conteo en cuadratura (con señal de dirección)

#### Implementación propuesta

```cpp
#include <driver/pcnt.h>

#define PCNT_UNIT       PCNT_UNIT_0
#define PCNT_CHANNEL    PCNT_CHANNEL_0
#define PCNT_FILTER_VAL 100  // Filtro ~1µs a 80MHz

namespace EncoderIR {

static int32_t _overflowCount = 0;  // Para manejar overflow del contador 16-bit

// ISR para overflow del contador PCNT
static void IRAM_ATTR _pcntOverflowISR(void* arg) {
    uint32_t status;
    pcnt_get_event_status(PCNT_UNIT, &status);
    if (status & PCNT_EVT_H_LIM) _overflowCount++;
    if (status & PCNT_EVT_L_LIM) _overflowCount--;
}

void begin() {
    // Configuración PCNT
    pcnt_config_t config = {
        .pulse_gpio_num = PIN_ENCODER_IR,
        .ctrl_gpio_num  = PCNT_PIN_NOT_USED,  // Sin señal de control
        .channel        = PCNT_CHANNEL,
        .unit           = PCNT_UNIT,
        .pos_mode       = PCNT_COUNT_INC,     // Incrementa en flanco positivo
        .neg_mode       = PCNT_COUNT_DIS,     // Ignora flanco negativo
        .lctrl_mode     = PCNT_MODE_KEEP,
        .hctrl_mode     = PCNT_MODE_KEEP,
        .counter_h_lim  = 10000,              // Límite superior
        .counter_l_lim  = -10000,             // Límite inferior
    };
    
    pcnt_unit_config(&config);
    
    // Filtro anti-glitch (~1.25µs)
    pcnt_set_filter_value(PCNT_UNIT, PCNT_FILTER_VAL);
    pcnt_filter_enable(PCNT_UNIT);
    
    // Configurar ISR para overflow
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_H_LIM);
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_L_LIM);
    pcnt_isr_service_install(0);
    pcnt_isr_handler_add(PCNT_UNIT, _pcntOverflowISR, NULL);
    
    // Iniciar contador
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    pcnt_counter_resume(PCNT_UNIT);
}

int32_t getGlobalPosition() {
    int16_t count;
    pcnt_get_counter_value(PCNT_UNIT, &count);
    return _overflowCount * 10000 + count;
}

void resetGlobalPosition() {
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    _overflowCount = 0;
    pcnt_counter_resume(PCNT_UNIT);
}

}  // namespace EncoderIR
```

#### Manejo de dirección con PCNT

Para detectar la dirección del movimiento se puede usar el pin de dirección del motor como señal de control:

```cpp
// Alternativa: Usar señal de dirección del motor como control
pcnt_config_t config = {
    .pulse_gpio_num = PIN_ENCODER_IR,
    .ctrl_gpio_num  = PIN_MOTOR_DIR,      // GPIO que indica dirección
    .channel        = PCNT_CHANNEL,
    .unit           = PCNT_UNIT,
    .pos_mode       = PCNT_COUNT_INC,     // Cuenta + cuando DIR=HIGH
    .neg_mode       = PCNT_COUNT_DIS,
    .lctrl_mode     = PCNT_MODE_REVERSE,  // Invierte cuando DIR=LOW
    .hctrl_mode     = PCNT_MODE_KEEP,
    // ...
};
```

---

### Opción B: Duplicar resolución con CHANGE

Cambio mínimo para duplicar los pulsos detectados.

```cpp
// Antes
attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_IR), _encoderISR, RISING);
#define PULSES_PER_REV   40

// Después
attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_IR), _encoderISR, CHANGE);
#define PULSES_PER_REV   80  // Duplicar
#define CM_POR_PULSO     0.0252f  // Mitad del valor anterior
```

**Ventajas:** Cambio mínimo de código  
**Desventajas:** No soluciona la pérdida de pulsos por latencia de ISR

---

### Opción C: Filtro RC + Schmitt Trigger (Hardware)

Agregar filtrado analógico antes del GPIO.

```
HC-020K OUT ──┬── R=1kΩ ──┬── 74HC14 ──── GPIO 16
              │           │   (Schmitt)
             C=100nF     GND
```

**Ventajas:** Elimina rebotes eléctricos  
**Desventajas:** Requiere hardware adicional, no soluciona latencia de ISR

---

## Plan de implementación

### Fase 1: Implementar PCNT (Opción A)

| Paso | Archivo | Cambio |
|------|---------|--------|
| 1 | `encoder_ir.cpp` | Reemplazar `attachInterrupt` por configuración PCNT |
| 2 | `encoder_ir.cpp` | Implementar ISR de overflow para contador extendido |
| 3 | `encoder_ir.cpp` | Usar `pcnt_get_counter_value()` en lugar de variable volátil |
| 4 | `encoder_ir.h` | Mantener interfaz pública sin cambios |
| 5 | `hardware_pins.h` | Agregar constante `PCNT_FILTER_VAL` |

### Fase 2: Validación

| Prueba | Método | Criterio de aceptación |
|--------|--------|------------------------|
| Conteo a baja velocidad | PWM 30%, medir 40 cm | Error < 1% |
| Conteo a alta velocidad | PWM 100%, medir 40 cm | Error < 2% |
| Repetibilidad | 10 ciclos ida/vuelta | Desviación < 0.5 cm |
| Latencia | Medir con otro encoder | PCNT cuenta igual o más |

### Fase 3: Ajuste de constantes

Recalibrar `CM_POR_PULSO` con el nuevo sistema:

```cpp
// Procedimiento:
// 1. Colocar carro en posición conocida (LS INICIO)
// 2. Mover carro a posición final (LS FIN)
// 3. Registrar pulsos totales
// 4. CM_POR_PULSO = distancia_real / pulsos_medidos
```

---

## Pruebas de validación

### Test 1: Comparación ISR vs PCNT

```cpp
// Habilitar ambos métodos temporalmente
volatile uint32_t isrCount = 0;
int16_t pcntCount = 0;

void loop() {
    // Mover carro de extremo a extremo
    // Comparar isrCount vs pcntCount
    Serial.printf("ISR: %lu, PCNT: %d, Diff: %ld\n", 
                  isrCount, pcntCount, (long)pcntCount - isrCount);
}
```

### Test 2: Frecuencia máxima

```cpp
// Medir frecuencia real de pulsos
static uint32_t lastCount = 0;
static unsigned long lastTime = 0;

void measureFrequency() {
    uint32_t count = EncoderIR::getPulseCount();
    unsigned long now = micros();
    
    if (now - lastTime >= 100000) {  // Cada 100ms
        uint32_t freq = (count - lastCount) * 10;  // Hz
        Serial.printf("Frecuencia: %lu Hz\n", freq);
        lastCount = count;
        lastTime = now;
    }
}
```

### Test 3: Repetibilidad de posición

Ejecutar desde el HMI:

1. Enviar carro a posición 0 cm (LS INICIO)
2. Mover a posición 45 cm
3. Regresar a posición 0 cm
4. Verificar que `pos_ir_cm` ≈ 0 (error < 0.5 cm)
5. Repetir 10 veces

---

## Referencias

- [ESP-IDF PCNT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/pcnt.html)
- [Datasheet LM393 Comparator](https://www.ti.com/lit/ds/symlink/lm393.pdf)
- [Calibración AS5600](./calibracion_posicion.md) — Sensor magnético complementario
- [Configuración Dual-Core](./config_dual_core.md) — Arquitectura de tareas FreeRTOS
