# Plan de Mejoras — Proyecto Péndulo Invertido (SPISC)

> **Fecha:** 20 de marzo de 2026  
> **Plataforma:** ESP32-WROOM-32 DevKitC · Driver BTS7960 · PlatformIO + Arduino  
> **Pista:** ≈ 95 cm de recorrido lineal

---

## Índice

1. [Estado Actual del Proyecto](#1-estado-actual-del-proyecto)
2. [Fase 1 — Limit Switches XV-152-1C25](#2-fase-1--limit-switches-xv-152-1c25)
3. [Fase 2 — Encoder Infrarrojo HC-020K en Motor](#3-fase-2--encoder-infrarrojo-hc-020k-en-motor)
4. [Fase 3 — Sensor MPU6050 sobre Carro Móvil](#4-fase-3--sensor-mpu6050-sobre-carro-móvil)
5. [Fase 4 — HMI Web (Vue.js + MQTT + FastAPI)](#5-fase-4--hmi-web-vuejs--mqtt--fastapi)
6. [Fase 5 — Arquitectura Modular de Modos de Control](#6-fase-5--arquitectura-modular-de-modos-de-control)
7. [Mapa Completo de GPIOs](#7-mapa-completo-de-gpios)
8. [Dependencias y Orden de Ejecución](#8-dependencias-y-orden-de-ejecución)
9. [Verificación por Fase](#9-verificación-por-fase)
10. [Decisiones de Diseño](#10-decisiones-de-diseño)

---

## 1. Estado Actual del Proyecto

### 1.1 Hardware actual

| Componente | Detalle |
|---|---|
| Microcontrolador | ESP32-WROOM-32 DevKitC |
| Motor driver | BTS7960 (H-Bridge dual, PWM 25 kHz, resolución 10-bit) |
| Encoder ángulo | AS5600 #1, magnético absoluto 12-bit, I2C Bus 0 (0x36) |
| Encoder posición | AS5600 #2, magnético acumulativo multi-vuelta, I2C Bus 1 (0x36) |
| Pantalla | OLED SSD1306 128×64, I2C Bus 0 (0x3C) |
| Comunicación | Serial UART 115200 baud (menú + debug) |

### 1.2 GPIOs en uso

| GPIO | Función | Periférico |
|------|---------|-----------|
| 1 | TX0 | Serial Monitor |
| 3 | RX0 | Serial Monitor |
| 12 | L_PWM | BTS7960 Canal 1, 25 kHz 10-bit |
| 13 | LED | Indicador debug |
| 14 | R_PWM | BTS7960 Canal 0, 25 kHz 10-bit |
| 18 | I2C SDA Bus 1 | AS5600 #2 (posición carro) + OLED SSD1306 |
| 19 | I2C SCL Bus 1 | AS5600 #2 + OLED |
| 21 | I2C SDA Bus 0 | AS5600 #1 (ángulo péndulo) |
| 22 | I2C SCL Bus 0 | AS5600 #1 |
| 26 | R_EN | BTS7960 Enable derecho |
| 27 | L_EN | BTS7960 Enable izquierdo |

### 1.3 Distribución I2C actual

| Bus | GPIOs | Dispositivos | Direcciones |
|-----|-------|-------------|-------------|
| Bus 0 (`TwoWire(0)`) | SDA 21 / SCL 22 | AS5600 #1 (ángulo) + OLED SSD1306 | 0x36, 0x3C |
| Bus 1 (`TwoWire(1)`) | SDA 18 / SCL 19 | AS5600 #2 (posición) | 0x36 |

### 1.4 Software existente

| Directorio / Archivo | Descripción |
|---|---|
| `SPISC/src/main.cpp` | Firmware principal: control neuronal FF + swing-up + menú serial |
| `SPISC/lib/Neural_Networks_FF/` | Librería propia de red neuronal feedforward con entrenamiento online |
| `SPISC/lib/Dynamic_Array/` | Librería de arreglos dinámicos (soporte para la red neuronal) |
| `Encoder_test/` | Proyecto de prueba para encoder HC-020K + motor TB6612FNG (ESP32-S3) |
| `scripts/control_neuronal.ino` | Script de control neuronal (versión .ino) |
| `scripts/control_neuronal.md` | Documentación completa del control neuronal |
| `scripts/pendulo_pid_final.ino` | Control PID cascada (ángulo + posición), no probado |
| `scripts/pendulo_lazo_abierto.ino` | Lazo abierto con logging a PLX-DAQ, no probado |
| `scripts/Reader_Pendulo_PR_GD.ino` | Lector de posición con PID simple, no probado |

### 1.5 Constantes clave del firmware actual

```
ANGLE_SETPOINT   = 180.0°       // Péndulo vertical hacia arriba
MIN_ANGLE        = 130.0°       // Límite seguridad inferior (180 - 50)
MAX_ANGLE        = 230.0°       // Límite seguridad superior (180 + 50)
MIN_POSITION     = 20.0 cm      // Límite izquierdo pista
MAX_POSITION     = 80.0 cm      // Límite derecho pista
posicionSetpoint = 45.0 cm      // Centro objetivo
ZONA_ALERTA      = 10.0 cm      // Zona crítica cerca de límites
KP_POS           = 0.5          // Ganancia lazo de posición
CM_POR_GRADO     = 0.0115780    // Conversión grados encoder → cm
SAMPLE_PERIOD    = 2000 µs      // Muestreo sensores (500 Hz)
PWM_FREQ         = 25000 Hz     // Frecuencia PWM motor
PWM_RES          = 10 bits      // Resolución PWM (0-1023)
```

> **Nota:** `MIN_POSITION` y `MAX_POSITION` se actualizarán dinámicamente tras la calibración con limit switches (Fase 1).

---

## 2. Fase 1 — Limit Switches XV-152-1C25

### 2.1 Objetivo

Instalar 2 interruptores de límite en los extremos de la pista (posición 0 y posición ≈95 cm) para:

1. **Calibración automática de posición:** Determinar el punto cero y la longitud real de la pista, permitiendo recalcular `CM_POR_GRADO` dinámicamente.
2. **Seguridad fail-safe:** Detener el motor inmediatamente si el carro alcanza un extremo de la pista, independientemente de la lectura de encoders o del estado de la conexión WiFi/MQTT.

### 2.2 Componente

**XV-152-1C25** — Micro switch de 3 pines:
- **C** (Común)
- **NO** (Normalmente Abierto)
- **NC** (Normalmente Cerrado)

### 2.3 Asignación de pines

| Limit Switch | GPIO | Justificación |
|---|---|---|
| **INICIO** (posición 0) | **GPIO 32** | Pull-up interno disponible, ADC1 (compatible WiFi), interrupt-capable |
| **FIN** (posición ≈95 cm) | **GPIO 33** | Pull-up interno disponible, ADC1 (compatible WiFi), interrupt-capable |

### 2.4 Conexión eléctrica

```
                ESP32
               ┌──────┐
               │  3V3  │───── No conectar directamente (pull-up es interno)
               │       │
  ┌─ NC ───────│ GP 32 │───── Limit Switch INICIO
  │            │       │
  └─ C ────────│  GND  │
               │       │
  ┌─ NC ───────│ GP 33 │───── Limit Switch FIN
  │            │       │
  └─ C ────────│  GND  │
               └──────┘

  Pin NO → sin conectar
```

**Configuración en código:**
```cpp
pinMode(PIN_LIMIT_START, INPUT_PULLUP);  // GPIO 32
pinMode(PIN_LIMIT_END,   INPUT_PULLUP);  // GPIO 33
```

**Lógica:**
- **Reposo (switch libre):** pin NC conectado a GND → lectura **LOW**... 

> **Corrección importante sobre la lógica con NC y Pull-Up:**
>
> Con la configuración NC + Pull-Up:
> - **Switch libre (no presionado):** NC está cerrado → el pin se conecta a GND a través de C → lectura **LOW**
> - **Switch presionado (carro tocando):** NC se abre → pull-up interno lleva el pin a HIGH → lectura **HIGH**
>
> Sin embargo, para **fail-safe** necesitamos que un cable cortado = motor frena. Con NC:
> - **Cable desconectado/cortado:** circuito abierto → pull-up lleva a **HIGH** → misma lectura que "switch presionado"
>
> Esto significa: si usamos NC, tanto "presionado" como "cable roto" dan **HIGH**, lo cual es el comportamiento fail-safe deseado.

**Lógica final:**

| Estado físico | Pin NC↔C | Lectura GPIO | Interpretación |
|---|---|---|---|
| Carro lejos del límite | Cerrado (NC) | **LOW** | ✅ Operación normal |
| Carro tocando el límite | Abierto | **HIGH** | ⚠️ ¡Límite alcanzado! → Frenar motor |
| Cable desconectado / roto | Abierto | **HIGH** | 🛑 Fail-safe → Frenar motor |

**Interrupción:** `RISING` (transición LOW→HIGH = switch activado).

### 2.5 Funcionalidades a implementar

#### A) Rutina de calibración de posición

```
1. Mover carro LENTAMENTE hacia Limit Switch INICIO (PWM bajo, ~200)
2. Al activarse LS_INICIO (RISING en GPIO 32):
   a. motorFreno()
   b. Resetear encoder posición: vueltas=0, posicionAcumulada=0, lastRawPos=0
   c. Registrar posición = 0 cm
3. Mover carro LENTAMENTE hacia Limit Switch FIN
4. Al activarse LS_FIN (RISING en GPIO 33):
   a. motorFreno()
   b. Registrar posición final en grados del encoder
   c. Calcular: CM_POR_GRADO = 95.0 / posicionAcumulada(grados)
   d. Calcular: MIN_POSITION y MAX_POSITION con margen de seguridad
5. Mover carro a posición central (posicionSetpoint)
6. Marcar calibración como completada
```

#### B) Seguridad por interrupción ISR

```cpp
void IRAM_ATTR isr_limit_start() {
    // Freno inmediato — se ejecuta independientemente del estado del programa
    ledcWrite(0, 0);  // R_PWM = 0
    ledcWrite(1, 0);  // L_PWM = 0
    limitStartTriggered = true;
}

void IRAM_ATTR isr_limit_end() {
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    limitEndTriggered = true;
}
```

### 2.6 Script de prueba

📄 **Archivo:** `scripts/test_limit_switches.ino`

**Objetivo:** Verificar conexión eléctrica y funcionamiento correcto de ambos limit switches antes de integrar al firmware principal.

**Funcionalidad:**
- Configura GPIO 32 y 33 como `INPUT_PULLUP`
- Registra interrupciones `RISING` en ambos pines
- Loop principal imprime estado de ambos switches cada 200 ms
- Al detectar activación por ISR: mensaje especial + enciende LED (GPIO 13)
- Permite probar ambos switches manualmente presionándolos

---

## 3. Fase 2 — Encoder Infrarrojo HC-020K en Motor

### 3.1 Objetivo

Instalar el encoder infrarrojo HC-020K directamente en el eje del motor DC del prototipo para medir RPM, velocidad y acumulación de pulsos. Complementa la medición de posición del AS5600 #2 con información de velocidad del motor.

### 3.2 Componente

**HC-020K** — Encoder óptico infrarrojo:
- Rueda ranurada de 20 slots → **PPR = 20**
- Salida digital: pulso HIGH por cada ranura detectada
- Alimentación: 5V (compatible con lógica 3.3V del ESP32)

### 3.3 Asignación de pin

| Sensor | GPIO | Justificación |
|---|---|---|
| **HC-020K (pulso)** | **GPIO 16** | Limpio, sin efectos de boot, interrupt-capable, disponible en WROOM-32 |

### 3.4 Conexión

```
  HC-020K          ESP32
  ┌──────┐        ┌──────┐
  │  VCC ├────────│  5V  │
  │  GND ├────────│  GND │
  │  OUT ├────────│ GP16 │  ← Señal de pulsos (interrupt RISING)
  └──────┘        └──────┘
```

### 3.5 Adaptación desde Encoder_test

El proyecto `Encoder_test/` fue probado con:
- **Board:** ESP32-S3-WROOM (diferente al prototipo)
- **Motor driver:** TB6612FNG (diferente al prototipo)
- **Pin encoder:** GPIO 10 (diferente)

**Cambios necesarios para adaptar:**

| Parámetro | Encoder_test (original) | Prototipo (adaptado) |
|---|---|---|
| Board | ESP32-S3-WROOM | ESP32-WROOM-32 |
| Pin encoder | GPIO 10 | **GPIO 16** |
| Motor driver | TB6612FNG (STBY, BIN1, BIN2, PWMB) | **BTS7960** (R_EN, L_EN, R_PWM, L_PWM) |
| PWM frecuencia | 10 kHz, 8-bit | **25 kHz, 10-bit** |
| Funciones motor | `motor_forward()`, `motor_reverse()` | **`motorAdelante()`**, **`motorAtras()`** |

**Lo que se conserva:**
- ISR `RISING` en pin de encoder
- PPR = 20
- Cálculo de RPS y RPM por ventana temporal
- Lectura atómica con `noInterrupts()`/`interrupts()`

### 3.6 Script de prueba

📄 **Archivo:** `scripts/test_encoder_ir_motor.ino`

**Objetivo:** Verificar que el encoder HC-020K funciona correctamente con el motor real del prototipo usando el driver BTS7960.

**Secuencia de prueba:**
```
Estado           | Duración | Motor         | PWM (10-bit)
─────────────────┼──────────┼───────────────┼─────────────
STOP_INIT        | 4 s      | Detenido      | 0
FWD_SLOW         | 4 s      | Adelante      | 300
FWD_MED          | 4 s      | Adelante      | 600
FWD_FAST         | 4 s      | Adelante      | 900
STOP_MID         | 4 s      | Detenido      | 0
REV_SLOW         | 4 s      | Atrás         | 300
REV_MED          | 4 s      | Atrás         | 600
REV_FAST         | 4 s      | Atrás         | 900
STOP_END         | 4 s      | Detenido      | 0
```

**Salida serial cada 500 ms:**
```
Estado       | PWM  | Dirección | Pulsos | Vueltas | RPS   | RPM
STOP_INIT    | 0    | DETENIDO  | 0      | 0.00    | 0.00  | 0.00
FWD_SLOW     | 300  | ADELANTE  | 120    | 6.00    | 6.00  | 360.00
...
```

---

## 4. Fase 3 — Sensor MPU6050 sobre Carro Móvil

### 4.1 Objetivo

Instalar un sensor IMU MPU6050 sobre el carro móvil para obtener:
- Aceleración lineal del carro (medición directa, alta fiabilidad)
- Velocidad angular del carro (medición directa, alta fiabilidad)
- Inclinación/orientación del carro (fusión sensorial, alta fiabilidad)

Esto complementa los datos del encoder magnético (posición) y el encoder IR (RPM del motor) con información inercial del carro.

### 4.2 Componente

**GY-521 (MPU6050)** — IMU de 6 ejes:
- Acelerómetro 3 ejes: ±2g, ±4g, ±8g, ±16g
- Giroscopio 3 ejes: ±250, ±500, ±1000, ±2000 °/s
- DMP (Digital Motion Processor) integrado
- Interfaz: I2C, dirección 0x68 (AD0→GND) o 0x69 (AD0→VCC)

### 4.3 Conexión I2C — Redistribución de buses

El MPU6050 se conecta al **Bus 0** (GPIO 21/22) y la **OLED se mueve al Bus 1** (GPIO 18/19):

#### Nueva distribución I2C (post Fase 3)

| Bus | GPIOs | Dispositivos | Direcciones |
|-----|-------|-------------|-------------|
| Bus 0 (`TwoWire(0)`) | SDA 21 / SCL 22 | AS5600 #1 (ángulo) + **MPU6050** | 0x36, **0x68** |
| Bus 1 (`TwoWire(1)`) | SDA 18 / SCL 19 | AS5600 #2 (posición) + **OLED SSD1306** | 0x36, **0x3C** |

**Justificación del cambio:**
- No hay conflicto de direcciones: MPU6050 (0x68) ≠ AS5600 (0x36)
- No hay conflicto de direcciones: OLED (0x3C) ≠ AS5600 (0x36)
- Se centraliza la IMU en Bus 0 junto con el sensor de ángulo
- No requiere pines GPIO adicionales

**Cambio en firmware:** La única línea que cambia es la inicialización de la OLED:
```cpp
// ANTES (Bus 0):
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cuno);

// DESPUÉS (Bus 1):
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cdos);
```

### 4.4 Conexión eléctrica

```
  MPU6050 (GY-521)     ESP32
  ┌──────────────┐    ┌──────┐
  │     VCC      ├────│ 3V3  │
  │     GND      ├────│ GND  │
  │     SCL      ├────│ GP22 │  ← I2C Bus 0 SCL (compartido con AS5600 #1)
  │     SDA      ├────│ GP21 │  ← I2C Bus 0 SDA (compartido con AS5600 #1)
  │     AD0      ├────│ GND  │  ← Dirección = 0x68
  │     INT      │    │      │  (Opcional: conectar a GPIO libre si se usa DMP)
  └──────────────┘    └──────┘
```

### 4.5 Qué puede medir el MPU6050

| Variable | Método | Fiabilidad | Utilidad |
|----------|--------|-----------|----------|
| **Aceleración lineal** (ax, ay, az) | Lectura directa acelerómetro | ✅ Alta | Medir aceleración del carro a lo largo de la pista |
| **Velocidad angular** (gx, gy, gz) | Lectura directa giroscopio | ✅ Alta | Detectar vibraciones, velocidad de rotación del carro |
| **Inclinación** (roll, pitch, yaw) | Filtro complementario o DMP | ✅ Alta | Detectar inclinación del carro, verificar nivel de pista |
| **Velocidad lineal** | Integración de aceleración | ⚠️ Media | Drift acumulado; útil solo en periodos cortos (<5 s) |
| **Posición lineal** | Doble integración | ❌ Baja | Drift severo; usar encoder para posición absoluta |

### 4.6 Enfoque recomendado

**Para el script de prueba:** Lecturas raw + **filtro complementario** (simple, rápido, suficiente para validar).

```cpp
// Filtro complementario para pitch y roll
pitch = 0.98 * (pitch + gy * dt) + 0.02 * atan2(ax, az) * RAD_TO_DEG;
roll  = 0.98 * (roll  + gx * dt) + 0.02 * atan2(ay, az) * RAD_TO_DEG;
```

**Para integración final:** Evaluar uso del **DMP** integrado del MPU6050 (fusión sensorial interna, quaterniones filtrados, mayor precisión, pero consume más memoria y requiere inicialización más larga).

**Librería recomendada:** `Electronic Cats MPU6050` (compatible PlatformIO, soporte DMP).

### 4.7 Script de prueba

📄 **Archivo:** `scripts/test_mpu6050.ino`

**Objetivo:** Verificar funcionamiento del MPU6050, leer 6 ejes, calcular orientación, y registrar datos mientras el carro se mueve.

**Secuencia:**
```
1. Reposo 3 s → establecer baseline de acelerómetro
2. Motor adelante PWM=300 por 5 s → medir aceleración
3. Parar 2 s → medir desaceleración
4. Motor atrás PWM=300 por 5 s → medir aceleración en reversa
5. Parar → fin de prueba
```

**Salida serial CSV (cada 50 ms):**
```
tiempo_ms, ax, ay, az, gx, gy, gz, pitch, roll
0, 0.02, -0.01, 9.81, 0.00, 0.00, 0.00, 0.1, -0.2
50, 0.85, -0.01, 9.80, 0.10, 0.00, 0.00, 0.1, -0.2
...
```

---

## 5. Fase 4 — HMI Web (Vue.js + MQTT + FastAPI)

### 5.1 Objetivo

Crear una interfaz HMI web que permita:
- Visualizar telemetría en tiempo real (gráficos + indicadores)
- Ver una animación del péndulo que refleje el estado real
- Controlar el prototipo (modos, calibración, parámetros, parada de emergencia)
- Registrar datos históricos para análisis posterior

### 5.2 Arquitectura de comunicación

```
┌─────────────────────────────────────────────────────────────────────┐
│                         RED WiFi LOCAL                               │
│                                                                      │
│  ┌──────────┐     MQTT (1883)      ┌──────────────┐                │
│  │  ESP32   ├──────────────────────►│  Mosquitto   │                │
│  │ Firmware │◄──────────────────────┤   Broker     │                │
│  └──────────┘    pendulum/cmd/*     │  (PC local)  │                │
│       │          pendulum/telemetry │              │                │
│       │                             └──────┬───────┘                │
│       │                                    │                         │
│       │                             ┌──────▼───────┐                │
│       │                             │   FastAPI    │                │
│       │                             │   Backend    │                │
│       │                             │  (Python)    │                │
│       │                             │  - aiomqtt   │                │
│       │                             │  - WebSocket │                │
│       │                             │  - SQLite    │                │
│       │                             └──────┬───────┘                │
│       │                                    │ WebSocket (:8000)       │
│       │                             ┌──────▼───────┐                │
│       │                             │   Vue.js     │                │
│       │                             │  Frontend    │                │
│       │                             │  (Vite)      │                │
│       │                             └──────────────┘                │
│       │                                                              │
│  El ESP32 mantiene safety local:                                    │
│  limit switches + timeout WiFi → motorFreno()                       │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.3 Tópicos MQTT

| Tópico | Dirección | Payload (JSON) | Frecuencia |
|--------|-----------|----------------|------------|
| `pendulum/telemetry` | ESP32 → Backend | `{angle, position, cart_accel, rpm, pwm, direction, ts}` | ~50 Hz |
| `pendulum/status` | ESP32 → Backend | `{state, mode, limits, errors, wifi_rssi}` | 1–5 Hz |
| `pendulum/limits` | ESP32 → Backend | `{start_switch, end_switch}` | On-change |
| `pendulum/cmd/motor` | Backend → ESP32 | `{action, pwm, direction}` | On-demand |
| `pendulum/cmd/mode` | Backend → ESP32 | `{mode, params}` | On-demand |
| `pendulum/cmd/calibrate` | Backend → ESP32 | `{type: "angle"\|"position"\|"full"}` | On-demand |
| `pendulum/cmd/config` | Backend → ESP32 | `{param, value}` | On-demand |
| `pendulum/cmd/stop` | Backend → ESP32 | `{}` | On-demand (EMERGENCIA) |

> **Rendimiento:** El ESP32 puede publicar MQTT a ~100 Hz sin problemas. Se recomienda publicar telemetría a ~50 Hz y usar interpolación en el frontend para animación fluida a 60 FPS.

### 5.4 Estructura de directorios propuesta

```
hmi/
├── backend/
│   ├── app.py                    # FastAPI: rutas REST + WebSocket
│   ├── mqtt_client.py            # Suscripción/publicación MQTT (aiomqtt)
│   ├── websocket_manager.py      # Gestión de conexiones WebSocket activas
│   ├── data_logger.py            # Logging a SQLite / CSV
│   ├── models.py                 # Schemas Pydantic (telemetría, comandos)
│   ├── config.py                 # Configuración: broker, puertos, tópicos
│   └── requirements.txt          # Dependencias Python
│
├── frontend/
│   ├── src/
│   │   ├── App.vue
│   │   ├── components/
│   │   │   ├── PendulumAnimation.vue   # Animación SVG/Canvas del péndulo
│   │   │   ├── TrackVisualization.vue  # Pista + carro + limit switches
│   │   │   ├── TelemetryCharts.vue     # Gráficos rolling (uPlot / Chart.js)
│   │   │   ├── StateIndicators.vue     # LEDs de estado (discretos)
│   │   │   ├── ControlPanel.vue        # Selector modo, START, STOP
│   │   │   ├── CalibrationPanel.vue    # Rutinas de calibración
│   │   │   ├── ParameterEditor.vue     # Editor KP, KI, KD, límites...
│   │   │   └── ConnectionStatus.vue    # Estado WiFi/MQTT
│   │   ├── composables/
│   │   │   ├── useWebSocket.js         # Lógica reactiva de WebSocket
│   │   │   └── useTelemetry.js         # Store de datos de telemetría
│   │   └── stores/
│   │       └── pendulumStore.js        # Pinia store central
│   ├── package.json
│   └── vite.config.js
│
└── mosquitto/
    └── mosquitto.conf                  # Configuración del broker
```

### 5.5 Componentes del Frontend

#### PendulumAnimation.vue — Animación en tiempo real
- **Canvas 2D o SVG** animado a 60 FPS
- Dibuja a escala proporcional (pista de 95 cm representada):
  - Base / pista con marcas de posición
  - Carro móvil (rectángulo) que se desplaza según `position`
  - Péndulo (barra + masa circular) que rota según `angle`
  - Limit switches en ambos extremos con indicador rojo/verde
- **Interpolación** entre muestras MQTT (~50 Hz) para suavizar a 60 FPS

#### TelemetryCharts.vue — Gráficos de variables continuas
- Ángulo del péndulo vs tiempo (rolling window ~30 s)
- Posición del carro vs tiempo
- PWM / velocidad del motor vs tiempo
- Aceleración del carro (MPU6050) vs tiempo
- RPM del encoder vs tiempo
- **Librería recomendada:** uPlot (alto rendimiento para >10k puntos) o Chart.js (más bonito)

#### StateIndicators.vue — Indicadores de estado discreto
- Estado del sistema: `IDLE`, `CALIBRATING`, `SWING_UP`, `CONTROLLING`, `EMERGENCY`
- Limit switch INICIO: 🟢 OK / 🔴 ACTIVADO
- Limit switch FIN: 🟢 OK / 🔴 ACTIVADO
- Conexión WiFi: barra RSSI
- Modo de control activo: nombre + estado
- Indicador de emergencia (LED rojo pulsante)

#### ControlPanel.vue — Panel de control
- **Selector de modo de control** (dropdown con todos los modos disponibles)
- **Botón INICIO** de rutina (verde)
- **Botón PARADA DE EMERGENCIA** (rojo, prominente, siempre visible)
- **Slider PWM manual** (solo activo en modo Manual)
- **Botones de dirección** (adelante/atrás, solo en modo Manual)

#### CalibrationPanel.vue — Calibración
- Botones: Calibrar Ángulo / Calibrar Posición / Calibración Completa
- Indicador de progreso de calibración
- Resultado: valores calibrados (angleOffset, CM_POR_GRADO, longitud pista)

#### ParameterEditor.vue — Editor de parámetros
- Edición en runtime de constantes ajustables:
  - `KP_POS`, `KP_ANGLE`, `KI_ANGLE`, `KD_ANGLE`
  - `MIN_ANGLE`, `MAX_ANGLE`, `MIN_POSITION`, `MAX_POSITION`
  - `ZONA_ALERTA`, `posicionSetpoint`
  - Parámetros específicos de cada modo de control
- Botón "Aplicar" → envía por MQTT al ESP32
- Botón "Restaurar defaults"

### 5.6 Cambios en firmware ESP32 para MQTT

**Librería:** `PubSubClient` (MQTT client para Arduino/ESP32)

**Flujo:**
1. Conectar WiFi (`WiFi.begin(SSID, PASSWORD)`)
2. Conectar broker MQTT (`client.connect(clientId)`)
3. Suscribirse a `pendulum/cmd/#`
4. En el loop principal:
   - Publicar JSON en `pendulum/telemetry` cada ~20 ms (50 Hz)
   - Publicar JSON en `pendulum/status` cada ~500 ms
   - Publicar en `pendulum/limits` solo al cambiar estado de switches
   - Procesar mensajes entrantes en callback MQTT
5. **Safety:** Si se pierde conexión WiFi/MQTT por >2 segundos → `motorFreno()`

> **Principio crítico:** La seguridad es **local al ESP32**. Los limit switches, timeouts y límites de ángulo/posición funcionan independientemente de WiFi/MQTT. La HMI es una herramienta de supervisión y control, pero el ESP32 se protege solo.

---

## 6. Fase 5 — Arquitectura Modular de Modos de Control

### 6.1 Objetivo

Refactorizar el firmware para soportar múltiples modos de control intercambiables en runtime, usando un patrón Strategy con máquina de estados.

### 6.2 Interfaz base — Patrón Strategy

```cpp
// control/control_mode.h
class ControlMode {
public:
    virtual ~ControlMode() {}

    // Inicializar el controlador con parámetros
    virtual void init() = 0;

    // Calcular salida de control: retorna PWM (0-1023) y dirección
    // sensors: {angle, position, cart_accel, rpm, dt, ...}
    virtual float compute(float angle, float position, float dt) = 0;

    // Reiniciar estados internos (errores acumulados, etc.)
    virtual void reset() = 0;

    // Nombre legible del modo
    virtual const char* getName() = 0;

    // Obtener/modificar parámetros en runtime
    virtual int getParamCount() = 0;
    virtual const char* getParamName(int index) = 0;
    virtual float getParam(int index) = 0;
    virtual void setParam(int index, float value) = 0;
};
```

### 6.3 Modos de control planificados

| # | Modo | Viabilidad ESP32 | Complejidad | Prioridad |
|---|------|------------------|-------------|-----------|
| 1 | **Manual / Lazo Abierto** | ✅ Trivial | Baja | 🔴 Alta |
| 2 | **PID Cascada** | ✅ Nativo | Baja | 🔴 Alta |
| 3 | **Red Neuronal FF** | ✅ Ya implementado | Media | 🟡 Media |
| 4 | **Régimen Deslizante (SMC)** | ✅ Viable | Media | 🟡 Media |
| 5 | **LQR** | ✅ Viable | Media | 🟡 Media |
| 6 | **Control Difuso (Fuzzy)** | ✅ Viable | Media-Alta | 🟢 Baja |
| 7 | **Control Adaptativo (MRAC)** | ⚠️ Viable con cuidado | Alta | 🟢 Baja |
| 8 | **MPC** | ⚠️ Limitado por CPU | Alta | 🟢 Baja |

### 6.4 Descripción de cada modo

#### 1. Manual / Lazo Abierto
- **Qué hace:** Control directo de PWM y dirección desde la HMI, sin retroalimentación automática.
- **Para qué sirve:** Caracterización del proceso ("system identification"). Dar impulsos al motor y observar cómo responden posición y ángulo en los gráficos. Registrar datos para modelado matemático.
- **Implementación:** Recibe PWM + dirección del tópico MQTT `pendulum/cmd/motor` y los aplica directamente. No computa nada. Los sensores siguen leyendo y enviando telemetría.

#### 2. PID Cascada (Ángulo + Posición)
- **Qué hace:** Lazo exterior PID de posición → genera setpoint de ángulo → lazo interior PID de ángulo → genera PWM.
- **Para qué sirve:** Control clásico robusto. Base de comparación para otros métodos.
- **Código base:** Ya existe parcialmente en `scripts/pendulo_pid_final.ino`.
- **Parámetros ajustables:** Kp, Ki, Kd de ambos lazos; límites de integrador.

#### 3. Red Neuronal Feedforward
- **Qué hace:** Red 3→3→1 con entrenamiento online (backpropagation en cada ciclo de control).
- **Para qué sirve:** Control adaptativo por aprendizaje. Ya implementado y probado.
- **Código base:** `SPISC/src/main.cpp` actual + `SPISC/lib/Neural_Networks_FF/`.
- **Parámetros:** Learning rate, estructura de red.

#### 4. Régimen Deslizante (Sliding Mode Control — SMC)
- **Qué hace:** Define una superficie de deslizamiento $s = \dot{e} + \lambda \cdot e$ y aplica una ley de control discontinua $u = -K \cdot \text{sign}(s)$.
- **Para qué sirve:** Muy robusto ante perturbaciones externas y errores de modelado. Ideal para péndulo invertido por su naturaleza inestable.
- **Chattering:** Se mitiga con función sigmoide o capa límite ("boundary layer") en vez de `sign()`.
- **Parámetros:** $\lambda$ (pendiente de superficie), $K$ (ganancia), ancho de capa límite.

#### 5. LQR (Linear Quadratic Regulator)
- **Qué hace:** Linealiza el modelo del péndulo alrededor de 180° y calcula una ganancia óptima $K$ resolviendo la ecuación de Riccati offline.
- **Ley de control:** $u = -K \cdot x$, donde $x = [\theta, \dot{\theta}, \text{pos}, \dot{\text{pos}}]$.
- **Para qué sirve:** Control óptimo clásico. Excelente rendimiento en la zona lineal (cerca de 180°).
- **Implementación:** La ganancia $K$ se calcula en MATLAB/Python y se carga como constantes en el ESP32. En runtime, es solo una multiplicación de vectores.
- **Parámetros:** Matrices Q (penalización estado) y R (penalización control), que determinan K.

#### 6. Control Difuso (Fuzzy Logic)
- **Qué hace:** Define funciones de pertenencia para error y derivada del error, con reglas IF-THEN tipo Mamdani o Sugeno.
- **Para qué sirve:** No requiere modelo matemático exacto. Funciona con conocimiento heurístico ("si el péndulo se inclina mucho a la derecha Y se está moviendo rápido, entonces aplicar fuerza fuerte a la izquierda").
- **Parámetros:** Funciones de pertenencia (centros, anchos), reglas, método de defuzzificación.
- **Ventaja HMI:** La tabla de reglas se puede editar visualmente desde la interfaz web.

#### 7. Control Adaptativo (MRAC — Model Reference Adaptive Control)
- **Qué hace:** Tiene un modelo de referencia de cómo debería comportarse el péndulo. Ajusta las ganancias del controlador online para minimizar el error entre el modelo de referencia y la planta real.
- **Para qué sirve:** Compensa cambios en el sistema (desgaste, cambio de carga, etc.).
- **Riesgo:** Mayor complejidad, posible inestabilidad si los parámetros de adaptación no están bien sintonizados.
- **Parámetros:** Ganancias de adaptación (γ), modelo de referencia.

#### 8. MPC (Model Predictive Control)
- **Qué hace:** En cada paso, predice la trayectoria futura del sistema resolviendo un problema de optimización con un horizonte de N pasos.
- **Limitación ESP32:** Horizonte corto (5–10 pasos) por capacidad de cómputo.
- **Alternativa:** Ejecutar MPC en el backend Python y enviar la acción de control vía MQTT. Añade latencia (~10–50 ms) pero permite modelos más complejos e horizontes más largos.
- **Parámetros:** Horizonte de predicción N, matrices de peso, restricciones.

### 6.5 Máquina de estados del firmware

```
                      ┌─────────────┐
                      │    IDLE      │ ← Estado inicial / después de STOP
                      └──────┬──────┘
                             │ cmd: CALIBRATE
                      ┌──────▼──────┐
                      │ CALIBRATING  │ ← Rutinas de calibración
                      └──────┬──────┘
                             │ calibración OK
                      ┌──────▼──────┐
                      │   READY      │ ← Listo, esperando selección de modo
                      └──────┬──────┘
                             │ cmd: START(mode)
                      ┌──────▼──────┐
                      │  SWING_UP    │ ← Solo si el modo lo requiere
                      └──────┬──────┘
                             │ ángulo dentro de ventana operativa
                      ┌──────▼──────┐
                      │ CONTROLLING  │ ← Modo activo ejecutándose
                      └──────┬──────┘
                             │ cmd: STOP / safety trigger
                      ┌──────▼──────┐
                      │  STOPPING    │ ← Frenado controlado
                      └──────┬──────┘
                             │ motor detenido
                      ┌──────▼──────┐
                      │    IDLE      │
                      └─────────────┘

  ⚡ EMERGENCY:  Desde CUALQUIER estado → motorFreno() → IDLE
     Triggers:   Limit switch activado
                 Pérdida WiFi por >2 s (si WiFi habilitado)
                 Ángulo fuera de rango de seguridad
                 Posición fuera de rango de seguridad
                 Comando STOP de emergencia desde HMI
```

### 6.6 Estructura modular del firmware

```
SPISC/
├── platformio.ini
├── include/
│   ├── config.h                    # Pines, constantes, WiFi credentials
│   ├── sensors.h                   # Lectura unificada (AS5600×2, MPU6050, encoder IR)
│   ├── motor.h                     # Abstracción BTS7960
│   ├── safety.h                    # Limit switches + verificaciones de seguridad
│   ├── communication.h             # WiFi + MQTT (PubSubClient)
│   ├── state_machine.h             # Estados del sistema (IDLE→...→CONTROLLING)
│   ├── display.h                   # OLED SSD1306 (ahora en Bus 1)
│   ├── calibration.h               # Rutinas de calibración (ángulo, posición, completa)
│   └── control/
│       ├── control_mode.h          # Clase base abstracta
│       ├── manual_mode.h           # Modo manual / lazo abierto
│       ├── pid_mode.h              # PID cascada
│       ├── neural_mode.h           # Red neuronal FF
│       ├── sliding_mode.h          # Régimen deslizante (SMC)
│       ├── lqr_mode.h              # LQR
│       ├── fuzzy_mode.h            # Control difuso
│       ├── adaptive_mode.h         # MRAC
│       └── mpc_mode.h              # MPC
├── src/
│   ├── main.cpp                    # Setup, loop, máquina de estados
│   ├── sensors.cpp
│   ├── motor.cpp
│   ├── safety.cpp
│   ├── communication.cpp
│   ├── state_machine.cpp
│   ├── display.cpp
│   ├── calibration.cpp
│   └── control/
│       ├── control_mode.cpp
│       ├── manual_mode.cpp
│       ├── pid_mode.cpp
│       ├── neural_mode.cpp
│       ├── sliding_mode.cpp
│       ├── lqr_mode.cpp
│       ├── fuzzy_mode.cpp
│       ├── adaptive_mode.cpp
│       └── mpc_mode.cpp
└── lib/
    ├── Dynamic_Array/
    └── Neural_Networks_FF/
```

---

## 7. Mapa Completo de GPIOs

### 7.1 Asignación final (todas las fases)

| GPIO | Función | Periférico | Fase |
|------|---------|-----------|------|
| 1 | TX0 | Serial Monitor | Existente |
| 3 | RX0 | Serial Monitor | Existente |
| 12 | L_PWM | BTS7960 Canal 1 | Existente |
| 13 | LED | Indicador debug | Existente |
| 14 | R_PWM | BTS7960 Canal 0 | Existente |
| **16** | **Encoder IR** | **HC-020K** (interrupt RISING) | **Fase 2** |
| 18 | I2C SDA Bus 1 | AS5600 #2 (posición) + OLED SSD1306 | Existente + Fase 3 (OLED reubicada) |
| 19 | I2C SCL Bus 1 | AS5600 #2 + OLED | Existente + Fase 3 |
| 21 | I2C SDA Bus 0 | AS5600 #1 (ángulo) + **MPU6050** | Existente + **Fase 3** |
| 22 | I2C SCL Bus 0 | AS5600 #1 + **MPU6050** | Existente + **Fase 3** |
| 26 | R_EN | BTS7960 Enable derecho | Existente |
| 27 | L_EN | BTS7960 Enable izquierdo | Existente |
| **32** | **Limit Switch INICIO** | **XV-152-1C25** (INPUT_PULLUP, ISR RISING) | **Fase 1** |
| **33** | **Limit Switch FIN** | **XV-152-1C25** (INPUT_PULLUP, ISR RISING) | **Fase 1** |

### 7.2 GPIOs libres para futuro

| GPIO | Notas |
|------|-------|
| 2 | Conectado a LED onboard en algunos DevKitC; usar con precaución |
| 4 | ADC2 (no disponible con WiFi activo); libre si no se necesita ADC |
| 5 | Pulso HIGH breve al boot; usable con precaución |
| 15 | PWM breve al boot; usable con precaución |
| 17 | Limpio, sin efectos de boot |
| 23 | Limpio |
| 25 | DAC1 |
| 34 | Solo entrada (sin pull-up interno) |
| 35 | Solo entrada (sin pull-up interno) |
| 36 (VP) | Solo entrada (sin pull-up interno) |
| 39 (VN) | Solo entrada (sin pull-up interno) |

---

## 8. Dependencias y Orden de Ejecución

### 8.1 Grafo de dependencias

```
Fase 1 (Limit Switches) ──┐
Fase 2 (Encoder IR)    ───┤──► Fase 4 (HMI) ──► Fase 5 (Modos de Control)
Fase 3 (MPU6050)       ───┘         ▲                     ▲
                                     │                     │
                        (puede iniciar con mocks)  (refactorización modular
                                                    puede iniciar en paralelo)
```

### 8.2 Orden sugerido

| Etapa | Fases | Actividades |
|-------|-------|-------------|
| **Etapa 1** | Fase 1 + 2 + 3 (paralelo) | Scripts de prueba de hardware. Validar cada sensor/actuador individualmente. |
| **Etapa 2** | Fase 4 — Backend | Configurar Mosquitto. Implementar FastAPI + MQTT + WebSocket. |
| **Etapa 3** | Fase 4 — Frontend | Vue.js: animación, gráficos, paneles de control. |
| **Etapa 4** | Fase 5 — Refactorización | Modularizar firmware actual. Implementar modo Manual + PID. |
| **Etapa 5** | Fase 5 — Modos avanzados | SMC, LQR, Neural (refactorizado), Fuzzy... iterativamente. |

---

## 9. Verificación por Fase

### 9.1 Fase 1 — Limit Switches

| # | Prueba | Resultado esperado |
|---|--------|-------------------|
| 1 | Ejecutar `test_limit_switches.ino`, presionar switch INICIO | Serial imprime "LS_INICIO: ACTIVADO", LED enciende |
| 2 | Soltar switch INICIO | Serial imprime "LS_INICIO: LIBRE", LED apaga |
| 3 | Presionar switch FIN | Serial imprime "LS_FIN: ACTIVADO", LED enciende |
| 4 | Desconectar cable de un switch | GPIO lee HIGH → fail-safe activado |

### 9.2 Fase 2 — Encoder IR

| # | Prueba | Resultado esperado |
|---|--------|-------------------|
| 1 | Motor adelante a PWM creciente | RPM incrementa proporcionalmente |
| 2 | Motor detenido | Pulsos = 0, RPM = 0 |
| 3 | Misma PWM adelante vs atrás | RPM similares (el encoder no distingue dirección) |

### 9.3 Fase 3 — MPU6050

| # | Prueba | Resultado esperado |
|---|--------|-------------------|
| 1 | Carro quieto | ax ≈ 0, ay ≈ 0, az ≈ ±9.81 m/s² |
| 2 | Mover carro | ax refleja aceleración en el eje de la pista |
| 3 | AS5600 #1 en mismo bus | Sigue leyendo ángulo correctamente (Bus 0) |
| 4 | OLED en Bus 1 | Sigue mostrando datos correctamente |

### 9.4 Fase 4 — HMI

| # | Prueba | Resultado esperado |
|---|--------|-------------------|
| 1 | ESP32 publica MQTT | `mosquitto_sub -t pendulum/telemetry` muestra JSON |
| 2 | Backend recibe datos | Log de FastAPI muestra telemetría procesada |
| 3 | Frontend conecta | Animación del péndulo responde a cambios reales |
| 4 | Comando STOP desde HMI | ESP32 frena motor inmediatamente |
| 5 | Desconexión WiFi | ESP32 frena motor por timeout (safety local) |

### 9.5 Fase 5 — Modos de Control

| # | Prueba | Resultado esperado |
|---|--------|-------------------|
| 1 | Modo Manual: PWM=200 adelante | Carro se mueve, telemetría refleja movimiento |
| 2 | Modo PID: iniciar control | Ángulo converge a 180° (vertical) |
| 3 | Cambio de modo en caliente | PID → Manual → PID sin reinicio del ESP32 |
| 4 | PARADA EMERGENCIA | Motor se detiene desde cualquier modo |

---

## 10. Decisiones de Diseño

| Decisión | Elección | Justificación |
|----------|----------|---------------|
| Microcontrolador | ESP32-WROOM-32 DevKitC | GPIO 16/17 disponibles (a diferencia del WROVER con PSRAM) |
| Motor driver | BTS7960 | Ya instalado en prototipo, H-bridge dual |
| LS — Pines | GPIO 32, 33 | ADC1 (compatibles con WiFi), pull-up interno, interrupt-capable |
| LS — Modo | NC (fail-safe) | Cable cortado = alarma activa (HIGH) |
| Encoder IR — Pin | GPIO 16 | Limpio, sin efectos de boot |
| MPU6050 — Bus I2C | Bus 0 (GPIO 21/22) | Dirección 0x68 ≠ AS5600 (0x36); sin conflicto |
| OLED — Bus I2C | Bus 1 (GPIO 18/19) | Movida de Bus 0 para dar espacio al MPU6050; dirección 0x3C ≠ AS5600 (0x36) |
| Frontend | Vue.js + Vite | Elegido por el usuario; curva de aprendizaje baja, ecosistema maduro |
| Comunicación | MQTT + Mosquitto | Elegido por el usuario; patrón pub/sub, robusto, escalable |
| Backend | FastAPI (Python) | Asíncrono, soporte WebSocket nativo, integra bien con aiomqtt |
| Safety | Local al ESP32 | Limit switches + timeout WiFi funcionan sin depender de la HMI |
| Telemetría | ~50 Hz MQTT | Suficiente para visualización; interpolación en frontend para 60 FPS |
| Modos de control | Patrón Strategy | Modular, intercambiable en runtime, fácil de extender |

---

## Apéndice A — Librerías PlatformIO necesarias

### Para el firmware SPISC (cuando todo esté integrado)

```ini
lib_deps =
    ; Existentes
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
    robtillaart/AS5600

    ; Fase 3 — MPU6050
    electroniccats/MPU6050

    ; Fase 4 — WiFi + MQTT
    knolleary/PubSubClient
    bblanchon/ArduinoJson
```

### Para el backend Python

```
fastapi
uvicorn[standard]
aiomqtt
pydantic
aiosqlite
websockets
```

### Para el frontend Vue.js

```
vue@3
vite
pinia
uplot          # o chart.js
```

---

## Apéndice B — Referencia rápida de archivos a crear

| Archivo | Fase | Propósito |
|---------|------|-----------|
| `scripts/test_limit_switches.ino` | 1 | Prueba básica de limit switches |
| `scripts/test_encoder_ir_motor.ino` | 2 | Prueba encoder IR con motor BTS7960 |
| `scripts/test_mpu6050.ino` | 3 | Prueba MPU6050 con movimiento del carro |
| `hmi/backend/` (directorio completo) | 4 | Backend FastAPI + MQTT |
| `hmi/frontend/` (directorio completo) | 4 | Frontend Vue.js |
| `hmi/mosquitto/mosquitto.conf` | 4 | Configuración broker MQTT |
| `SPISC/` (refactorización modular) | 5 | Firmware modularizado con modos de control |
