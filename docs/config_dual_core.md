# Configuración Dual-Core del ESP32

**Proyecto:** [SPISC — Sistema de Péndulo Invertido con Supervisión y Control](../README.md)  
**Fecha:** 30/03/2026  
**Autor:** Ing. Jeisson Martínez Flórez

---

## Tabla de contenidos

- [Descripción general](#descripción-general)
- [Arquitectura implementada](#arquitectura-implementada)
- [Configuración de tareas FreeRTOS](#configuración-de-tareas-freertos)
- [Mecanismos de sincronización](#mecanismos-de-sincronización)
- [Problemas encontrados](#problemas-encontrados)
- [Soluciones implementadas](#soluciones-implementadas)
- [Pruebas realizadas](#pruebas-realizadas)
- [Resultados obtenidos](#resultados-obtenidos)
- [Parámetros configurables](#parámetros-configurables)
- [Recomendaciones de optimización](#recomendaciones-de-optimización)

---

## Descripción general

El ESP32 cuenta con dos núcleos (Core 0 y Core 1) que pueden ejecutar tareas en paralelo. Esta implementación separa las operaciones críticas de tiempo real (sensores y control) de las operaciones flexibles de comunicación (MQTT, OLED), eliminando la interferencia mutua y garantizando un loop de control estable a 500 Hz.

### Motivación

Antes de la implementación dual-core, el sistema single-threaded presentaba:
- Jitter en el loop de sensores cuando MQTT reconectaba (~2-5 segundos de bloqueo)
- Actualización de OLED (92ms) afectando la frecuencia de muestreo
- Pérdida de lecturas de sensores durante transmisiones WiFi

---

## Arquitectura implementada

### Distribución de cores

```
┌─────────────────────────────────────────────────────────────┐
│  CORE 0 — Sensores y Control (500 Hz, crítico)             │
│    - Lectura de sensores cada 2ms                          │
│    - Procesamiento de comandos MQTT (desde cola)           │
│    - Verificación de seguridad                             │
│    - Ejecución de modos de control (SWEEP_PWM, OPEN_LOOP)  │
│    - Hardware watchdog                                      │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│  CORE 1 — Comunicación (flexible, no crítico)              │
│    - MQTT keep-alive y reconexión automática               │
│    - Publicación de telemetría (5 Hz)                      │
│    - Publicación de estado (2 Hz)                          │
│    - Actualización de OLED (2 Hz)                          │
└─────────────────────────────────────────────────────────────┘
```

### Flujo de datos entre cores

```
┌──────────────┐      ┌─────────────────────────┐
│   CORE 0     │      │        CORE 1           │
│              │      │                         │
│  Sensores    │─────▶│  TelemetryPacket       │──▶ MQTT Publish
│  update()    │ mutex│  (copia protegida)     │
│              │      │                         │
│  onCommand() │◀─────│  MQTT Callback         │
│  (procesa)   │ cola │  (encola comando)      │
└──────────────┘      └─────────────────────────┘
```

---

## Configuración de tareas FreeRTOS

### Constantes de configuración

Archivo: [`SPISC/src/main.cpp`](../SPISC/src/main.cpp)

```cpp
// Core 0 - Sensores y Control
#define SENSORS_TASK_PERIOD_MS   2      // 500 Hz
#define SENSORS_TASK_STACK    4096      // bytes
#define SENSORS_TASK_PRIORITY    5      // Alta prioridad

// Core 1 - Comunicación
#define COMM_TASK_STACK       8192      // bytes (JSON + WiFi buffers)
#define COMM_TASK_PRIORITY       3      // Prioridad media
#define COMM_TASK_PERIOD_MS     10      // Yield cada 10ms

// Watchdog
#define WATCHDOG_TIMEOUT_S       5      // Segundos
```

### Creación de tareas

```cpp
// Tarea de sensores en CORE 0 (prioridad alta)
xTaskCreatePinnedToCore(
    taskSensorsAndControl,      // Función
    "Sensors",                  // Nombre
    SENSORS_TASK_STACK,         // Stack (bytes)
    NULL,                       // Parámetros
    SENSORS_TASK_PRIORITY,      // Prioridad 5
    &taskSensorsHandle,         // Handle
    0                           // Core 0
);

// Tarea de comunicación en CORE 1 (prioridad media)
xTaskCreatePinnedToCore(
    taskCommunication,          // Función
    "Comm",                     // Nombre
    COMM_TASK_STACK,            // Stack mayor para JSON + WiFi
    NULL,                       // Parámetros
    COMM_TASK_PRIORITY,         // Prioridad 3
    &taskCommHandle,            // Handle
    1                           // Core 1
);
```

---

## Mecanismos de sincronización

### 1. Mutex para telemetría (datos compartidos)

Archivo: [`SPISC/include/telemetry_data.h`](../SPISC/include/telemetry_data.h)

```cpp
extern SemaphoreHandle_t telemetryMutex;

void initTelemetryMutex();
bool lockTelemetry(uint32_t waitMs = 10);
void unlockTelemetry();
```

**Uso en Core 0 (escritura):**
```cpp
if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
    buildTelemetry(sharedTelemetry);
    xSemaphoreGive(telemetryMutex);
}
```

**Uso en Core 1 (lectura):**
```cpp
TelemetryPacket pkt;
if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    pkt = sharedTelemetry;  // Copia local
    xSemaphoreGive(telemetryMutex);
}
MqttComm::publishTelemetry(pkt);
```

### 2. Mutex I2C para Bus 1 (Wire1)

El Bus I2C 1 es compartido entre:
- **Core 0:** AS5600 carro (sensor de posición magnética)
- **Core 1:** OLED SSD1306 (display)

Archivo: [`SPISC/include/sensors_i2c.h`](../SPISC/include/sensors_i2c.h)

```cpp
extern SemaphoreHandle_t i2cBus1Mutex;

void initI2CMutex();
bool lockI2CBus1(uint32_t waitMs = 50);
void unlockI2CBus1();
```

**Uso en sensores (Core 0):**
```cpp
bool gotMutex = lockI2CBus1(10);  // Esperar máximo 10ms
if (gotMutex) {
    int32_t cumPos = _as5600_cart.getCumulativePosition();
    unlockI2CBus1();
    // procesar datos...
}
```

**Uso en OLED (Core 1):**
```cpp
if (!SensorsI2C::lockI2CBus1(30)) {
    return;  // Omitir actualización si no hay mutex
}
_display->clearDisplay();
// ... dibujar contenido ...
_display->display();
SensorsI2C::unlockI2CBus1();
```

### 3. Cola de comandos MQTT

Archivo: [`SPISC/include/mqtt_comm.h`](../SPISC/include/mqtt_comm.h)

```cpp
struct MqttCommand {
    char topic[64];
    char payload[256];
};

void initCommandQueue();
bool enqueueCommand(const char* topic, const char* payload);
void processCommandQueue();
```

**Flujo:**
1. MQTT callback (Core 1) → `enqueueCommand()` → cola FreeRTOS
2. Loop sensores (Core 0) → `processCommandQueue()` → ejecuta `onMqttCommand()`

---

## Problemas encontrados

### Problema 1: Conflicto I2C entre cores (crítico)

**Síntoma:** Sistema entraba en loop infinito de recovery I2C, luego watchdog reset.

```
[sensors] I2C recovery (SDA=18 SCL=19)
[sensors] I2C recovery (SDA=18 SCL=19)
... (80+ veces repetido)
E (21053) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (21053) task_wdt:  - Sensors (CPU 0)
```

**Causa raíz:** Wire1 (Bus 1) era accedido simultáneamente por:
- Core 0: `SensorsI2C::update()` → AS5600 carro (cada 2ms)
- Core 1: `DisplayOLED::update()` → OLED (cada 500ms)

Sin mutex, ambos cores corrompían el estado del bus I2C, disparando errores continuos y recovery infinito.

### Problema 2: Watchdog se disparaba durante recovery I2C

**Síntoma:** Después de muchos intentos de I2C recovery, el watchdog mataba la tarea de sensores.

**Causa:** `feedWatchdog()` estaba al **final** del loop de sensores. Si el recovery I2C bloqueaba por más de 5 segundos, el watchdog no se alimentaba.

### Problema 3: "Timeout homing" falso positivo

**Síntoma:**
```
[openloop] Home (inicio) alcanzado.
[openloop] Timeout homing inicio.  ← ¡Inmediatamente después!
```

**Causa:** Underflow de aritmética unsigned.

```cpp
// now se captura al inicio del loop (T1 = 50000)
unsigned long now = millis();

// Durante el loop, se detecta home y se cambia estado
_changeState(OL_READY);  // _stateEntry = millis() (T2 = 50001)

// Verificación de timeout usa 'now' capturado antes
if ((now - _stateEntry) > 30000) {  // 50000 - 50001 = underflow = ~4.29 billones
    // ¡Se dispara incorrectamente!
}
```

### Problema 4: Timeout MQTT muy agresivo

**Síntoma:**
```
[safety] Timeout MQTT — frenando motor
[safety] Timeout MQTT — frenando motor
```

**Causa:** Timeout de 2000ms era insuficiente para tolerar reconexiones WiFi normales (cuando el cliente se desconectaba momentáneamente del AP).

---

## Soluciones implementadas

### Solución 1: Mutex I2C para Bus 1

Archivo: [`SPISC/src/sensors_i2c.cpp`](../SPISC/src/sensors_i2c.cpp)

```cpp
SemaphoreHandle_t i2cBus1Mutex = NULL;

void initI2CMutex() {
    if (i2cBus1Mutex == NULL) {
        i2cBus1Mutex = xSemaphoreCreateMutex();
    }
}

bool lockI2CBus1(uint32_t waitMs) {
    if (i2cBus1Mutex == NULL) return true;
    return xSemaphoreTake(i2cBus1Mutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

void unlockI2CBus1() {
    if (i2cBus1Mutex != NULL) {
        xSemaphoreGive(i2cBus1Mutex);
    }
}
```

### Solución 2: Limitar intentos de recovery I2C

```cpp
static const int I2C_MAX_RECOVERY_ATTEMPTS = 3;
static int _recoveryAttempts1 = 0;

// En update():
if (hadError) {
    _i2cErrorCount1++;
    if (_i2cErrorCount1 >= I2C_ERROR_THRESHOLD && 
        _recoveryAttempts1 < I2C_MAX_RECOVERY_ATTEMPTS) {
        if (lockI2CBus1(20)) {
            _recoverI2CBus(Wire1, I2C_BUS1_SDA, I2C_BUS1_SCL);
            unlockI2CBus1();
        }
        _recoveryAttempts1++;
    }
} else {
    _i2cErrorCount1 = 0;
    _recoveryAttempts1 = 0;  // Reset en lectura exitosa
}
```

### Solución 3: Watchdog al inicio del loop

```cpp
void taskSensorsAndControl(void* pvParameters) {
    for (;;) {
        // 0. Alimentar watchdog PRIMERO (evita timeout si I2C se bloquea)
        Safety::feedWatchdog();

        // 1. Leer sensores
        SensorsI2C::update();
        // ... resto del loop
    }
}
```

### Solución 4: Break inmediato en transición de estado (homing)

```cpp
case OL_HOMING_START:
    if (startActive) {
        Motor::freno();
        Serial.println("[openloop] Home (inicio) alcanzado.");
        _changeState(OL_READY);
        break;  // ← Salir inmediatamente para evitar underflow de timeout
    }
    // Safety timeout solo si NO cambió estado
    if ((now - _stateEntry) > 30000) {
        Motor::freno();
        Serial.println("[openloop] Timeout homing inicio.");
        _changeState(OL_STOPPED);
    }
    break;
```

### Solución 5: Aumentar timeout MQTT

```cpp
// En setup():
Safety::setMqttTimeout(5000);  // 5 segundos (antes era 2s)
```

---

## Pruebas realizadas

### Prueba 1: Estabilidad dual-core básica

**Objetivo:** Verificar que el sistema arranca y mantiene ambos cores operativos sin crashes.

**Procedimiento:**
1. Flashear firmware
2. Conectar WiFi y MQTT
3. Observar logs por 5+ minutos

**Resultado esperado:** Sin watchdog resets, sin reboots.

### Prueba 2: Operación de modo OPEN_LOOP

**Objetivo:** Verificar comandos de impulso y homing con arquitectura dual-core.

**Procedimiento:**
1. Enviar comando home
2. Ejecutar múltiples impulsos (variando PWM, duración, dirección)
3. Verificar detección de limit switches

**Comandos MQTT enviados:**
```json
{"action":"home"}
{"action":"impulse","pwm":570,"duration":0.4}
{"action":"impulse","pwm":680,"duration":0.2,"direction":"forward"}
{"action":"impulse","pwm":680,"duration":0.2,"direction":"backward"}
```

### Prueba 3: Stress de comunicación

**Objetivo:** Verificar que reconexiones WiFi no afectan el control.

**Procedimiento:**
1. Ejecutar impulsos mientras el cliente WiFi se desconecta/reconecta
2. Observar comportamiento del motor y mensajes de safety

---

## Resultados obtenidos

### Prueba inicial (antes de fix I2C mutex)

| Aspecto | Resultado |
|---------|-----------|
| Estabilidad | ❌ Watchdog reset después de ~20 segundos |
| I2C recovery | ❌ Loop infinito (~80+ intentos) |
| Causa | Conflicto de bus I2C entre cores |

**Log de error:**
```
[sensors] I2C recovery (SDA=18 SCL=19)
... (repetido 80+ veces)
E (21053) task_wdt: Task watchdog got triggered
```

### Prueba después de fix I2C mutex

| Aspecto | Resultado |
|---------|-----------|
| Estabilidad | ✅ Sin crashes ni reboots |
| I2C | ✅ Sin recovery spam |
| Homing | ❌ "Timeout homing" falso positivo |
| MQTT timeout | ❌ Mensajes frecuentes de timeout |

### Prueba final (todos los fixes aplicados)

| Aspecto | Resultado |
|---------|-----------|
| Estabilidad | ✅ Operación continua sin problemas |
| Homing | ✅ "Home alcanzado" sin timeout falso |
| Impulsos | ✅ Múltiples impulsos ejecutados correctamente |
| Limit switches | ✅ Detección correcta ("Impulso detenido por LS") |
| MQTT timeout | ✅ Sin mensajes espurios durante operación normal |
| I2C | ✅ Un error inicial al boot (normal), recuperación automática |

**Log de operación exitosa:**
```
[MqttBridge] MQTT conectado!
[main] Cambio de modo → OPEN_LOOP
[openloop] Modo Lazo Abierto activado.
[openloop] Impulso PWM=570 dur=400ms dir=ADELANTE
[openloop] Impulso completado (400ms).
[openloop] Home (fin) alcanzado.
[openloop] Impulso PWM=680 dur=400ms dir=ATRAS
[openloop] Impulso completado (400ms).
[openloop] Home (inicio) alcanzado.
[openloop] Impulso PWM=590 dur=300ms dir=ADELANTE
[openloop] Impulso completado (300ms).
```

### Métricas de operación

| Métrica | Valor |
|---------|-------|
| Frecuencia de sensores | 500 Hz (2ms período) |
| Frecuencia de telemetría MQTT | 5 Hz (200ms período) |
| Frecuencia de estado MQTT | 2 Hz (500ms período) |
| Frecuencia de OLED | 2 Hz (500ms período) |
| Timeout MQTT safety | 5000 ms |
| Timeout watchdog | 5 segundos |
| Max recovery I2C por ciclo | 3 intentos |

---

## Parámetros configurables

### Archivo: `SPISC/src/main.cpp`

| Parámetro | Valor actual | Rango recomendado | Efecto |
|-----------|-------------|-------------------|--------|
| `SENSORS_TASK_PERIOD_MS` | 2 | 1-10 | Frecuencia de muestreo (500-100 Hz) |
| `SENSORS_TASK_PRIORITY` | 5 | 4-6 | Prioridad vs otras tareas |
| `COMM_TASK_PRIORITY` | 3 | 2-4 | Prioridad de comunicación |
| `WATCHDOG_TIMEOUT_S` | 5 | 3-10 | Tiempo antes de reset |
| `Safety::setMqttTimeout()` | 5000 | 3000-10000 | Timeout sin actividad MQTT |

### Archivo: `SPISC/src/sensors_i2c.cpp`

| Parámetro | Valor actual | Rango recomendado | Efecto |
|-----------|-------------|-------------------|--------|
| `I2C_ERROR_THRESHOLD` | 5 | 3-10 | Errores antes de recovery |
| `I2C_MAX_RECOVERY_ATTEMPTS` | 3 | 2-5 | Intentos máximos por ciclo |
| `lockI2CBus1(waitMs)` | 10/30/50 | 5-100 | Tiempo de espera por mutex |

### Archivo: `SPISC/include/wifi_mqtt_config.h`

| Parámetro | Valor actual | Efecto |
|-----------|-------------|--------|
| `TELEMETRY_INTERVAL_MS` | 200 | Frecuencia de telemetría (5 Hz) |
| `STATUS_INTERVAL_MS` | 500 | Frecuencia de estado (2 Hz) |

---

## Recomendaciones de optimización

### 1. Separar OLED a bus I2C dedicado (hardware)

**Problema actual:** Wire1 es compartido entre AS5600 carro y OLED, requiriendo mutex.

**Mejora propuesta:** Mover OLED a Wire (Bus 0) usando multiplexor I2C TCA9548A, o usar interface SPI para OLED.

**Beneficio:** Elimina contención de mutex, mejora latencia de ambos dispositivos.

### 2. Aumentar frecuencia de telemetría

**Configuración actual:** 5 Hz (200ms)

**Propuesta:** Subir a 10-20 Hz para mejor resolución temporal en análisis de oscilaciones del péndulo.

**Consideración:** Verificar que MQTT/WiFi no se sature. Posible solución: usar formato binario en vez de JSON.

### 3. Implementar buffer circular para telemetría

**Problema:** Si MQTT se desconecta, se pierden datos de telemetría.

**Propuesta:** Buffer circular en RAM (500-1000 muestras) que se vacía cuando MQTT reconecta.

### 4. Ajustar prioridades según carga

**Experimento sugerido:**
- Bajar `COMM_TASK_PRIORITY` a 2 si hay jitter en sensores
- Subir `SENSORS_TASK_PRIORITY` a 6 para máxima prioridad (excepto WiFi interno)

### 5. Optimizar actualización OLED

**Problema actual:** `display()` bloquea ~23ms a 400kHz (antes era 92ms a 100kHz).

**Propuestas:**
- Reducir frecuencia de OLED a 1 Hz
- Actualizar solo campos que cambiaron (dirty flags)
- Usar OLED con interface SPI (más rápido)

### 6. Telemetría selectiva por modo

**Propuesta:** Publicar diferentes campos según el modo activo:
- `MODE_IDLE`: Solo estado básico (1 Hz)
- `MODE_OPEN_LOOP`: Posición + ángulo (10 Hz)
- `MODE_PID`: Todos los campos incluyendo MPU (20 Hz)

---

## Archivos modificados

| Archivo | Cambios |
|---------|---------|
| `SPISC/src/main.cpp` | Arquitectura dual-core, tareas FreeRTOS |
| `SPISC/include/telemetry_data.h` | Mutex de telemetría |
| `SPISC/src/telemetry_data.cpp` | Implementación mutex |
| `SPISC/include/sensors_i2c.h` | API mutex I2C Bus1 |
| `SPISC/src/sensors_i2c.cpp` | Mutex I2C, límite de recovery |
| `SPISC/src/display_oled.cpp` | Uso de mutex para Wire1 |
| `SPISC/include/mqtt_comm.h` | Cola de comandos |
| `SPISC/src/mqtt_comm.cpp` | Implementación cola FreeRTOS |
| `SPISC/include/safety.h` | API watchdog hardware |
| `SPISC/src/safety.cpp` | Implementación watchdog ESP32 |
| `SPISC/src/modes/lazo_abierto_mode.cpp` | Fix timeout homing |

---

## Referencias

- [ESP32 FreeRTOS Task API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)
- [ESP32 Task Watchdog](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html)
- [FreeRTOS Mutex](https://www.freertos.org/Real-time-embedded-RTOS-Mutexes.html)
- [Plan Dual-Core Original](plan_dual_core.md)
