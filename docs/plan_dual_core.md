# Plan: Arquitectura Dual-Core para ESP32

## Resumen Ejecutivo (No Técnico)

**¿Qué vamos a hacer?**  
La ESP32 tiene dos "cerebros" (núcleos de procesamiento), pero actualmente solo estamos usando uno. Vamos a distribuir el trabajo entre ambos para que el sistema sea más rápido y estable.

**¿Por qué es importante?**  
Actualmente, cuando el sistema envía datos por WiFi, los sensores pueden retrasarse hasta 92 milisegundos. En un péndulo invertido que necesita leer sensores 500 veces por segundo, esto causa inestabilidad.

**¿Cuál es el beneficio?**  
- Lecturas de sensores sin interrupciones (estabilidad del control)
- Comunicación WiFi/MQTT sin afectar el control del motor
- Sistema más responsivo y predecible

**Riesgo:**  
Medio. Requiere reestructurar el código, pero no cambia el hardware.

---

## Análisis Técnico

### Estado Actual

```
┌─────────────────────────────────────────────────┐
│                    CORE 1                        │
│  Arduino loop() - TODO el código usuario        │
│  - Sensores I2C (500 Hz)                        │
│  - MQTT loop + publish                          │
│  - OLED update (bloquea 23-92ms)                │
│  - Safety checks                                │
│  - Modos de control                             │
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│                    CORE 0                        │
│  WiFi stack interno (automático)                │
│  - TCP/IP processing                            │
│  - WiFi management                              │
└─────────────────────────────────────────────────┘
```

### Problemas Identificados

| Problema | Ubicación | Impacto |
|----------|-----------|---------|
| OLED bloquea 23-92ms | `display_oled.cpp:update()` | Sensores pierden ventana de 2ms |
| MQTT en loop principal | `main.cpp` línea 247 | Retrasa lecturas de sensores |
| JSON serialization | `mqtt_comm.cpp:publishTelemetry()` | CPU-intensive, 768 bytes cada 200ms |
| Calibración MPU | `sensors_i2c.cpp:calibrateMPU()` | Bloquea 200ms |
| Safety sin watchdog hardware | `safety.cpp` | Si loop bloquea >2s, motor para |

### Arquitectura Propuesta

```
┌─────────────────────────────────────────────────┐
│              CORE 0 — Control (Crítico)          │
│  Prioridad: Alta (configPRIORITY: 5)            │
│  Stack: 4096 bytes                               │
│                                                  │
│  Tarea: taskSensorsAndControl                   │
│  - SensorsI2C::update()     @ 500 Hz            │
│  - EncoderIR::updateRPM()   @ 500 Hz            │
│  - LimitSwitches::check..() @ 500 Hz            │
│  - Safety::check()          @ 500 Hz            │
│  - Modos activos loop()     @ 500 Hz            │
│                                                  │
│  Período: 2ms (deadline estricto)               │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│              CORE 1 — Comunicación               │
│  Prioridad: Media (configPRIORITY: 3)           │
│  Stack: 8192 bytes (JSON + WiFi buffers)        │
│                                                  │
│  Tarea: taskCommunication                       │
│  - MqttComm::loop()         continuo            │
│  - publishTelemetry()       @ 5 Hz              │
│  - publishStatus()          @ 2 Hz              │
│  - DisplayOLED::update()    @ 2 Hz              │
│                                                  │
│  Período: flexible (no crítico)                 │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│              Datos Compartidos                   │
│  (Protegidos con Mutex o Atomic)                │
│                                                  │
│  - TelemetryPacket (struct compartida)          │
│  - FirmwareMode activeMode                      │
│  - Comandos MQTT pendientes (cola)              │
└─────────────────────────────────────────────────┘
```

---

## Archivos a Modificar

| Archivo | Cambios |
|---------|---------|
| `SPISC/src/main.cpp` | Reemplazar `setup()`/`loop()` por tareas FreeRTOS |
| `SPISC/include/telemetry_data.h` | Añadir mutex para `TelemetryPacket` compartido |
| `SPISC/src/mqtt_comm.cpp` | Añadir cola de comandos thread-safe |
| `SPISC/include/mqtt_comm.h` | Exponer cola de comandos |
| `SPISC/src/safety.cpp` | Añadir watchdog hardware como backup |

---

## Pasos de Implementación

### Fase 1: Estructura de Datos Compartidos

**Paso 1.1: Crear estructura de telemetría thread-safe**

Archivo: `SPISC/include/telemetry_data.h`

Añadir después de la definición de `TelemetryPacket`:

```cpp
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Mutex para acceso thread-safe a telemetría
extern SemaphoreHandle_t telemetryMutex;

// Funciones helper thread-safe
void initTelemetryMutex();
bool lockTelemetry(TickType_t wait = portMAX_DELAY);
void unlockTelemetry();
```

**Paso 1.2: Crear cola de comandos MQTT**

Archivo: `SPISC/include/mqtt_comm.h`

Añadir:

```cpp
#include <freertos/queue.h>

// Estructura para comandos en cola
struct MqttCommand {
    char topic[64];
    char payload[512];
};

// Cola de comandos (MQTT → Core 0)
extern QueueHandle_t mqttCommandQueue;

// Inicializar cola
void initCommandQueue();

// Encolar comando (llamado desde callback MQTT en Core 1)
bool enqueueCommand(const char* topic, const char* payload);

// Procesar comandos pendientes (llamado desde Core 0)
void processCommandQueue();
```

### Fase 2: Crear Tareas FreeRTOS

**Paso 2.1: Modificar `main.cpp`**

Reemplazar el contenido del archivo con la nueva arquitectura:

```cpp
#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ... (includes existentes)

// ══════════════════════════════════════════════════
// Handles de tareas
// ══════════════════════════════════════════════════
static TaskHandle_t taskSensorsHandle = NULL;
static TaskHandle_t taskCommHandle    = NULL;

// ══════════════════════════════════════════════════
// Datos compartidos
// ══════════════════════════════════════════════════
static TelemetryPacket sharedTelemetry = {};
SemaphoreHandle_t telemetryMutex = NULL;
QueueHandle_t mqttCommandQueue = NULL;

// ══════════════════════════════════════════════════
// TAREA CORE 0: Sensores y Control (500 Hz)
// ══════════════════════════════════════════════════
void taskSensorsAndControl(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(2);  // 2ms = 500 Hz

    for (;;) {
        // 1. Leer sensores
        SensorsI2C::update();
        EncoderIR::updateRPM();
        
        // 2. Verificar limit switches
        LimitSwitches::checkAndPublishChanges();
        
        // 3. Safety check
        Safety::check();
        
        // 4. Procesar comandos MQTT pendientes
        MqttComm::processCommandQueue();
        
        // 5. Ejecutar modo activo
        switch (activeMode) {
            case MODE_SWEEP_PWM:  RangoPwmMode::loop(); break;
            case MODE_OPEN_LOOP:  LazoAbiertoMode::loop(); break;
            default: break;
        }
        
        // 6. Actualizar telemetría compartida
        if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
            buildTelemetry(sharedTelemetry);
            xSemaphoreGive(telemetryMutex);
        }
        
        // Esperar hasta siguiente período (timing preciso)
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

// ══════════════════════════════════════════════════
// TAREA CORE 1: Comunicación (flexible)
// ══════════════════════════════════════════════════
void taskCommunication(void* pvParameters) {
    unsigned long lastTelemetryMs = 0;
    unsigned long lastStatusMs    = 0;
    unsigned long lastDisplayMs   = 0;

    for (;;) {
        unsigned long now = millis();
        
        // 1. MQTT keep-alive (no bloquea Core 0)
        MqttComm::loop();
        
        // Mantener safety alive mientras MQTT está conectado
        if (MqttComm::isConnected()) {
            Safety::markMqttActivity();
        }
        
        // 2. Publicar telemetría @ 5 Hz
        if ((now - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
            lastTelemetryMs = now;
            
            TelemetryPacket pkt;
            if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                pkt = sharedTelemetry;  // Copia local
                xSemaphoreGive(telemetryMutex);
            }
            MqttComm::publishTelemetry(pkt);
        }
        
        // 3. Publicar estado @ 2 Hz
        if ((now - lastStatusMs) >= STATUS_INTERVAL_MS) {
            lastStatusMs = now;
            
            TelemetryPacket pkt;
            if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                pkt = sharedTelemetry;
                xSemaphoreGive(telemetryMutex);
            }
            
            MqttComm::publishStatus(
                getActiveModeStateStr(), activeMode,
                pkt.ls_start, pkt.ls_end,
                MqttComm::getWifiRssi(),
                SensorsI2C::isAngleCalibrated(),
                SensorsI2C::isMagPosCalibrated(),
                SensorsI2C::isMPUCalibrated());
        }
        
        // 4. Actualizar OLED @ 2 Hz (puede bloquear 92ms, no afecta sensores)
        if ((now - lastDisplayMs) >= 500) {
            lastDisplayMs = now;
            TelemetryPacket pkt;
            if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                pkt = sharedTelemetry;
                xSemaphoreGive(telemetryMutex);
            }
            DisplayOLED::update(pkt, getActiveModeStateStr());
        }
        
        // Yield para no monopolizar CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ══════════════════════════════════════════════════
// Setup
// ══════════════════════════════════════════════════
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[main] ═══ SPISC — Firmware Dual-Core ═══");

    // Inicializar hardware (igual que antes)
    Motor::begin();
    Serial.println("[main] Motor OK");
    
    EncoderIR::begin();
    Serial.println("[main] Encoder IR OK");
    
    LimitSwitches::begin();
    Serial.println("[main] Limit switches OK");
    
    SensorsI2C::begin();
    Serial.println("[main] Sensores I2C OK");
    
    if (DisplayOLED::begin()) {
        DisplayOLED::showMessage("SPISC Dual-Core", "Iniciando...");
    }

    // Crear mutex y cola
    telemetryMutex = xSemaphoreCreateMutex();
    MqttComm::initCommandQueue();
    Serial.println("[main] Mutex y cola OK");

    // Inicializar MQTT con callback que encola comandos
    MqttComm::onCommand(onMqttCommand);
    if (MqttComm::begin()) {
        Serial.println("[main] WiFi + MQTT OK");
        DisplayOLED::showMessage("SPISC Dual-Core", "MQTT conectado!");
    } else {
        Serial.println("[main] WiFi no conectó (reintentará)");
        DisplayOLED::showMessage("SPISC Dual-Core", "WiFi pendiente...");
    }

    // Seguridad
    Safety::setMqttTimeout(2000);
    Serial.println("[main] Safety OK");

    // Modo inicial
    activeMode = MODE_IDLE;

    // ═══ CREAR TAREAS EN CORES SEPARADOS ═══
    
    // Tarea de sensores en CORE 0 (prioridad alta)
    xTaskCreatePinnedToCore(
        taskSensorsAndControl,  // Función
        "Sensors",              // Nombre
        4096,                   // Stack (bytes)
        NULL,                   // Parámetros
        5,                      // Prioridad (alta)
        &taskSensorsHandle,     // Handle
        0                       // Core 0
    );
    Serial.println("[main] Tarea Sensors creada en Core 0");

    // Tarea de comunicación en CORE 1 (prioridad media)
    xTaskCreatePinnedToCore(
        taskCommunication,
        "Comm",
        8192,                   // Stack mayor para JSON + WiFi
        NULL,
        3,                      // Prioridad media
        &taskCommHandle,
        1                       // Core 1
    );
    Serial.println("[main] Tarea Comm creada en Core 1");

    Serial.println("[main] Sistema dual-core activo. Esperando comandos...\n");
}

void loop() {
    // Vacío - todo corre en tareas FreeRTOS
    vTaskDelay(portMAX_DELAY);
}
```

### Fase 3: Modificar Callback MQTT

**Paso 3.1: Encolar comandos en lugar de procesarlos directamente**

Archivo: `SPISC/src/mqtt_comm.cpp`

Añadir funciones para manejo de cola:

```cpp
// Al inicio del archivo, después de los includes
#include <freertos/queue.h>

// Variable estática para la cola
static QueueHandle_t _commandQueue = NULL;

// Implementación de funciones de cola
void initCommandQueue() {
    _commandQueue = xQueueCreate(10, sizeof(MqttCommand));
}

bool enqueueCommand(const char* topic, const char* payload) {
    if (!_commandQueue) return false;
    
    MqttCommand cmd;
    strncpy(cmd.topic, topic, sizeof(cmd.topic) - 1);
    cmd.topic[sizeof(cmd.topic) - 1] = '\0';
    strncpy(cmd.payload, payload, sizeof(cmd.payload) - 1);
    cmd.payload[sizeof(cmd.payload) - 1] = '\0';
    
    return xQueueSend(_commandQueue, &cmd, 0) == pdTRUE;
}

void processCommandQueue() {
    if (!_commandQueue) return;
    
    MqttCommand cmd;
    while (xQueueReceive(_commandQueue, &cmd, 0) == pdTRUE) {
        // Llamar al callback registrado
        if (_commandCallback) {
            _commandCallback(cmd.topic, cmd.payload);
        }
    }
}
```

**Paso 3.2: Modificar el callback interno de MqttBridge**

En el callback de MqttBridge, en lugar de llamar directamente al handler, encolar:

```cpp
// En MqttBridge.cpp, modificar _staticCallback
void MqttBridge::_staticCallback(char* topic, byte* payload, unsigned int length) {
    if (!_instance) return;

    const unsigned int maxLen = 511;
    unsigned int safeLen = (length < maxLen) ? length : maxLen;
    char buf[512];
    memcpy(buf, payload, safeLen);
    buf[safeLen] = '\0';

    // En lugar de llamar directamente, encolar para Core 0
    MqttComm::enqueueCommand(topic, buf);
}
```

### Fase 4: Añadir Watchdog Hardware

**Paso 4.1: Backup de seguridad con Task Watchdog**

Archivo: `SPISC/src/safety.cpp`

Añadir al inicio:

```cpp
#include "esp_task_wdt.h"

static bool _wdtInitialized = false;
```

Añadir funciones:

```cpp
void initHardwareWatchdog(uint32_t timeoutSeconds) {
    if (_wdtInitialized) return;
    
    esp_task_wdt_init(timeoutSeconds, true);  // panic on timeout
    esp_task_wdt_add(NULL);  // añadir tarea actual
    _wdtInitialized = true;
    
    Serial.printf("[safety] Watchdog hardware iniciado (%ds)\n", timeoutSeconds);
}

void feedWatchdog() {
    if (_wdtInitialized) {
        esp_task_wdt_reset();
    }
}
```

**Paso 4.2: Llamar feedWatchdog() en el loop de sensores**

En `taskSensorsAndControl`, al final de cada iteración:

```cpp
// Al final del for(;;) loop
Safety::feedWatchdog();
```

---

## Verificación

### Tests a Realizar

1. **Test de timing de sensores:**
   ```
   - Medir tiempo entre lecturas de SensorsI2C::update()
   - Debe ser consistente: 2ms ± 0.1ms
   - Sin picos de >5ms
   ```

2. **Test de latencia MQTT:**
   ```
   - Enviar comando desde backend
   - Medir tiempo hasta respuesta de telemetría
   - Debe ser <50ms (actualmente puede ser >100ms)
   ```

3. **Test de OLED sin afectar sensores:**
   ```
   - Monitorear timing de sensores mientras OLED actualiza
   - OLED puede tardar 92ms sin afectar los 2ms de sensores
   ```

4. **Test de reconexión WiFi:**
   ```
   - Desconectar WiFi del router
   - Verificar que sensores siguen funcionando
   - Motor mantiene último comando o para por safety
   ```

### Métricas de Éxito

| Métrica | Antes | Después |
|---------|-------|---------|
| Jitter sensores | 0-92ms | <0.5ms |
| Latencia comando MQTT | 50-150ms | <30ms |
| Bloqueo por OLED | 92ms afecta todo | 0ms en Core 0 |
| Uso CPU Core 0 | ~80% | ~40% |
| Uso CPU Core 1 | ~20% | ~60% |

---

## Dependencias y Riesgos

### Dependencias
- No requiere cambios de hardware
- No requiere librerías adicionales (FreeRTOS incluido en ESP-IDF)

### Riesgos

| Riesgo | Probabilidad | Mitigación |
|--------|--------------|------------|
| Race conditions en datos compartidos | Media | Usar mutex + copias locales |
| Deadlock por mutex | Baja | Timeouts en xSemaphoreTake |
| Stack overflow en tareas | Baja | Monitorear con uxTaskGetStackHighWaterMark() |
| Prioridades incorrectas | Baja | Core 0 siempre prioridad mayor |

---

## Debugging

### Monitorear uso de stack

Añadir temporalmente en loop de comunicación:

```cpp
Serial.printf("[debug] Stack Sensors: %d bytes libres\n", 
              uxTaskGetStackHighWaterMark(taskSensorsHandle));
Serial.printf("[debug] Stack Comm: %d bytes libres\n", 
              uxTaskGetStackHighWaterMark(taskCommHandle));
```

### Monitorear timing de sensores

Añadir en `taskSensorsAndControl`:

```cpp
static unsigned long maxLoopTime = 0;
unsigned long loopStart = micros();

// ... código del loop ...

unsigned long loopTime = micros() - loopStart;
if (loopTime > maxLoopTime) {
    maxLoopTime = loopTime;
    Serial.printf("[timing] Max loop: %lu us\n", maxLoopTime);
}
```

---

## Rollback

Si la implementación falla, revertir a arquitectura single-core:

1. `git checkout SPISC/src/main.cpp`
2. `git checkout SPISC/src/mqtt_comm.cpp`
3. `git checkout SPISC/include/mqtt_comm.h`
4. Recompilar

El código original está versionado en git.
