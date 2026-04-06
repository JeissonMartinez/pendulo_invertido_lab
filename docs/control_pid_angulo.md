# Control PID de Ángulo del Péndulo

**Proyecto:** [SPISC — Sistema de Péndulo Invertido con Supervisión y Control](../README.md)  
**Fecha:** 04/04/2026  
**Última actualización:** 06/04/2026 (v1.2 — bias de motor + re-tuning de ganancias)  
**Autor:** Ing. Jeisson Martínez Flórez

---

## Tabla de contenidos

- [Descripción general](#descripción-general)
- [Arquitectura del modo](#arquitectura-del-modo)
- [Máquina de estados](#máquina-de-estados)
- [Comandos MQTT](#comandos-mqtt)
- [Parámetros configurables](#parámetros-configurables)
- [Algoritmo PID](#algoritmo-pid)
- [Rango de trabajo y convención angular](#rango-de-trabajo-y-convención-angular)
- [Telemetría](#telemetría)
- [Comunicación MQTT](#comunicación-mqtt)
- [Guía de uso](#guía-de-uso)
- [Troubleshooting](#troubleshooting)
- [Estado actual del sistema](#estado-actual-del-sistema)
- [Mejoras futuras propuestas](#mejoras-futuras-propuestas)
- [Referencias](#referencias)

---

## Descripción general

El modo **Control PID de Ángulo** (`MODE_PID_ANG`) implementa un controlador PID discreto para estabilizar el péndulo invertido en posición vertical (0°). Este modo:

- Incluye un PID de ángulo (control principal) para estabilización vertical
- Incorpora un PID de posición interno para pre-posicionamiento y reposicionamiento automático del carro al centro del riel (45 cm)
- Detecta automáticamente la entrada/salida del rango de trabajo angular configurable
- Maneja la transición angular 360°/0° mediante normalización del error a [-180°, +180°]
- Expone telemetría extendida con error, integral, derivada y salida del controlador (campos `pida_*`)
- Permite configuración dinámica de ganancias Kp, Ki, Kd y parámetros de control vía MQTT

### Principio de funcionamiento

```
┌─────────────────────────────────────────────────────────────┐
│  Flujo de Control PID Ángulo                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Setpoint (0°) ──┬──► (+) ──► Controlador  ──► Motor      │
│   (vertical)      │     │      PID Ángulo        PWM       │
│                   │     │                                   │
│                   │   (-)                                   │
│                   │     │                                   │
│   AS5600 ─────────┴─────┘                                   │
│   (ángulo 0°-360°, normalizado a ±180°)                     │
│                                                             │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ PID Posición interno (pre-pos / reposicionamiento)     │ │
│  │   Setpoint = 45 cm (centro del riel)                   │ │
│  │   Sensor: HC-020K IR                                   │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Características principales

| Característica | Valor por defecto | Rango configurable |
|---------------|-------------------|---------------------|
| **Frecuencia de control** | 1000 Hz (Core 0) | Fijo |
| **Sensor de ángulo** | AS5600 magnético | Fijo |
| **Rango angular** | 0° - 360° | Fijo |
| **Setpoint angular** | 0° (vertical arriba) | Fijo |
| **Rango de trabajo** | ±20° | [5°, 30°] |
| **Tolerancia angular** | 0.1° | [0°, 5°] |
| **PWM mínimo** | 250 | [100, 500] |
| **PWM máximo** | 900 | [300, 1023] |
| **Anti-windup** | Saturación de integral ±500 | Fijo |
| **Filtro derivativo** | Pasa-bajos α = 0.2 | Fijo |
| **Motor bias** | 300 PWM (zona muerta) | Fijo |
| **Pos. pre-posicionamiento** | 45 cm (centro riel) | Fijo |
| **Telemetría (PID activo)** | ~14-17 Hz (fast) | Fijo 50ms target |
| **Telemetría (reposo)** | 5 Hz (normal) | Fijo 200ms |

---

## Arquitectura del modo

### Archivos involucrados

```
SPISC/
├── src/modes/
│   ├── pid_angulo_mode.h      ← Header con interfaz pública
│   └── pid_angulo_mode.cpp    ← Implementación del modo (~530 líneas)
├── include/
│   ├── telemetry_data.h       ← Enum MODE_PID_ANG + 15 campos pida_* en TelemetryPacket
│   ├── hardware_pins.h        ← TELEMETRY_INTERVAL_MS, TELEMETRY_FAST_INTERVAL_MS
│   └── wifi_mqtt_config.h     ← TOPIC_CMD_PID_ANG "pendulum/cmd/pidang"
├── src/
│   ├── main.cpp               ← Orquestador dual-core (7 puntos de integración)
│   ├── mqtt_comm.cpp           ← Suscripción + serialización JSON pida_*
│   └── encoder_ir.cpp          ← Posición con dirección motor/magnético
└── lib/
    └── MqttBridge/             ← Conexión WiFi+MQTT (setNoDelay)
```

### Integración en main.cpp (7 puntos)

1. **Include**: `#include "modes/pid_angulo_mode.h"`
2. **switchMode()**: Case `MODE_PID_ANG` → `PidAnguloMode::begin()`
3. **getStateStr()**: Case `MODE_PID_ANG` → `PidAnguloMode::getStateStr()`
4. **Command routing (cmd/pidang)**: → `PidAnguloMode::onCommand()`
5. **Command routing (cmd/stop)**: → `PidAnguloMode::onCommand()`
6. **buildTelemetry()**: Llena 15 campos `pida_*` con getters + setea `telemetry_fast`
7. **taskControl loop**: Case `MODE_PID_ANG` → `PidAnguloMode::loop()`

### Namespace

```cpp
namespace PidAnguloMode {
    void begin();               // Inicializa modo (auto-homing)
    void loop();                // Loop de control (1000 Hz en Core 0)
    void onCommand(...);        // Procesa comandos MQTT
    const char* getStateStr();  // Estado como string
    bool isActive();            // ¿Motor en movimiento?
    
    // Getters para telemetría (15 campos pida_*)
    float getError();           // Error angular normalizado (±180°)
    float getIntegral();        // Término integral acumulado
    float getDerivada();        // Término derivativo filtrado
    float getOutput();          // Salida PID cruda
    float getOutputSaturated(); // Salida PID saturada (PWM aplicado)
    float getSetpoint();        // Siempre 0.0° (vertical)
    float getKp(), getKi(), getKd();
    float getRangoTrabajo();    // Rango de trabajo en grados
    float getTolerancia();      // Tolerancia angular en grados
    int   getPwmMin(), getPwmMax();
    float getPosSetpoint();     // Setpoint posición interno (45 cm)
}
```

---

## Máquina de estados

```
┌──────────────────────────────────────────────────────────────────────┐
│                    Diagrama de Estados PID_ANG                       │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌──────────┐  cmd:run    ┌─────────────────┐                      │
│   │PIDA_IDLE │────────────►│PIDA_HOMING_START│                      │
│   └──────────┘  (auto)     └────────┬────────┘                      │
│        │                            │                                │
│        │ ya en home                 │ LS_START activo                │
│        │                            ▼                                │
│        │                   ┌────────────────┐                        │
│        │                   │PIDA_CALIBRATING│  (200ms pausa)         │
│        │                   └───────┬────────┘                        │
│        │                           │                                 │
│        │                           ▼                                 │
│        │                 ┌───────────────────┐                       │
│        │                 │PIDA_PREPOSITIONING│  PID pos → 45 cm      │
│        │                 └────────┬──────────┘                       │
│        │                          │                                  │
│        │           pos OK (500ms settle)                             │
│        │                          │                                  │
│        │                          ▼                                  │
│        └──────────────────►┌────────────┐                            │
│                            │ PIDA_READY │◄────────────────────┐      │
│   ┌──────────────┐         └─────┬──────┘                     │      │
│   │PIDA_HOMING   │◄─────────────┤ cmd:end                    │      │
│   │    _END      │               │                            │      │
│   └──────┬───────┘               │ ángulo entra              │      │
│          │                       │ en rango de trabajo        │      │
│          │ LS_END activo         ▼                            │      │
│          └──────────────►┌──────────────┐                     │      │
│                          │ PIDA_RUNNING │  PID ángulo 1000Hz  │      │
│                          └──────┬───────┘                     │      │
│                                 │                             │      │
│          ┌──────────────────────┼──────────────────┐          │      │
│          │                      │                  │          │      │
│          ▼                      ▼                  ▼          │      │
│   ┌─────────────┐    ┌──────────────────┐  ┌─────────────┐   │      │
│   │PIDA_STOPPED │    │péndulo fuera de  │  │límite físico│   │      │
│   │  (cmd:stop) │    │rango de trabajo  │  │alcanzado    │   │      │
│   └──────┬──────┘    └───────┬──────────┘  └──────┬──────┘   │      │
│          │                   │                    │           │      │
│          │                   ▼                    ▼           │      │
│          │           ┌───────────────────┐                    │      │
│          │           │PIDA_REPOSITIONING │  PID pos → 45 cm  │      │
│          │           └────────┬──────────┘                    │      │
│          │                    │                               │      │
│          │                    │ pos OK (500ms settle)         │      │
│          │                    └───────────────────────────────┘      │
│          │                                                           │
│          └──► cmd:reset → PIDA_IDLE                                  │
│              cmd:config → actualiza parámetros                       │
│              cmd:home/end → posicionamiento manual                   │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

### Descripción de estados

| Estado | Descripción |
|--------|-------------|
| `PIDA_IDLE` | Estado inicial. Auto-inicia homing si no está en LS_START. Acepta `config`, `home`, `end`, `run`. |
| `PIDA_HOMING_START` | Motor retrocediendo (PWM 300) hasta LS_START. Timeout: 30s. |
| `PIDA_HOMING_END` | Motor avanzando (PWM 300) hasta LS_END. Timeout: 30s. |
| `PIDA_CALIBRATING` | Pausa breve (200ms) para estabilización de sensores post-homing. Auto-calibración de posición a 0 cm via main.cpp. |
| `PIDA_PREPOSITIONING` | PID de posición interno mueve carro a 45 cm (centro del riel). Tolerancia 0.5 cm, settle time 500ms. |
| `PIDA_READY` | Esperando que el usuario levante el péndulo al rango de trabajo. **Auto-transiciona a RUNNING** cuando el ángulo del péndulo entra en la banda definida por `rango_trabajo`. |
| `PIDA_RUNNING` | Control PID de ángulo activo a 1000 Hz. Telemetría rápida activada. Si el péndulo sale del rango de trabajo → freno + reposicionamiento. |
| `PIDA_REPOSITIONING` | Péndulo fuera de rango → PID posición interno reposiciona carro a 45 cm → vuelve a READY. |
| `PIDA_STOPPED` | Detenido por comando `stop`, límite físico o zona de peligro. Acepta `config`, `home`, `end`, `run`, `reset`. |

### Flujo de la secuencia `run`

```
cmd:run → HOMING_START → LS_START → CALIBRATING (200ms)
       → PREPOSITIONING (PID pos → 45cm, settle 500ms)
       → READY (esperando péndulo vertical)
       → RUNNING (PID ángulo activo 1000 Hz)
       ↔ REPOSITIONING (si péndulo sale de rango)
       ↔ READY (esperando péndulo de vuelta)
```

---

## Comandos MQTT

**Tópico:** `pendulum/cmd/pidang`

### Tabla de comandos

| Acción | Payload JSON | Desde estados | Descripción |
|--------|--------------|---------------|-------------|
| `home` | `{"action":"home"}` | IDLE, READY, STOPPED | Mover a LS_START (home) |
| `end` | `{"action":"end"}` | IDLE, READY, STOPPED | Mover a LS_END |
| `config` | Ver debajo | IDLE, READY, STOPPED | Configurar parámetros PID |
| `run` | `{"action":"run"}` | IDLE, READY, STOPPED | Iniciar secuencia completa (homing→calibración→pre-pos→ready) |
| `stop` | `{"action":"stop"}` | Cualquiera | Detener inmediatamente (freno + reset variables PID) |
| `reset` | `{"action":"reset"}` | Cualquiera | Volver a PIDA_IDLE |

> **Nota:** El tópico global `pendulum/cmd/stop` también detiene este modo inmediatamente.

### Payload de configuración

```json
{
  "action": "config",
  "kp": 40.0,
  "ki": 1.0,
  "kd": 0.5,
  "rango_trabajo": 20,
  "tolerancia": 0.1,
  "pwm_min": 250,
  "pwm_max": 900
}
```

> **Nota:** Todos los parámetros son opcionales. Solo se actualizan los incluidos en el payload. Los parámetros se validan y clampean a sus rangos válidos.

### Diferencias con PID Posición

| Aspecto | PID Posición (`pidpos`) | PID Ángulo (`pidang`) |
|---------|------------------------|-----------------------|
| Setpoint | Configurable (cm) | Fijo 0° (vertical) |
| Sensor de control | HC-020K IR / AS5600 (seleccionable) | AS5600 (ángulo, fijo) |
| Zona de seguridad | Configurable (cm desde límites) | Fija (5 cm) — solo para PID pos. interno |
| Rango de trabajo | N/A | Configurable (°) — banda para activar/desactivar PID |
| Estado AT_TARGET | Sí (setpoint alcanzado) | No (control continuo mientras esté en rango) |
| Pre-posicionamiento | No | Sí (PID posición interno a 45 cm) |
| Reposicionamiento | No | Sí (automático al salir del rango de trabajo) |
| Selector de sensor | Sí (mag/ir) | No |

---

## Parámetros configurables

| Parámetro | Default | Rango | Unidad | Descripción |
|-----------|---------|-------|--------|-------------|
| `kp` | 40.0 | [0.1, 100.0] | - | Ganancia proporcional |
| `ki` | 1.0 | [0.0, 50.0] | - | Ganancia integral |
| `kd` | 0.5 | [0.0, 10.0] | - | Ganancia derivativa |
| `rango_trabajo` | 20.0 | [5.0, 30.0] | ° | Banda angular desde 0° para activar/desactivar PID |
| `tolerancia` | 0.1 | [0.0, 5.0] | ° | Zona muerta angular (freno si \|error\| < tolerancia) |
| `pwm_min` | 250 | [100, 500] | - | PWM mínimo aplicado al motor |
| `pwm_max` | 900 | [300, 1023] | - | PWM máximo aplicado al motor |

### Validación de parámetros

Los parámetros se validan al recibirse por MQTT:
- Se clampean al rango válido (no se rechazan)
- Si `pwm_min >= pwm_max`, ambos se resetean a valores por defecto (250, 900)
- Los parámetros se aplican inmediatamente solo en estados no-activos (IDLE, READY, STOPPED)

### Parámetros fijos del PID posición interno

El PID de posición interno (para pre-posicionamiento y reposicionamiento) usa parámetros fijos no configurables vía MQTT:

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| Kp | 6.0 | Ganancia proporcional |
| Ki | 1.5 | Ganancia integral |
| Kd | 0.0 | Ganancia derivativa |
| Setpoint | 45.0 cm | Centro del riel |
| Tolerancia | 0.5 cm | Error aceptable |
| Zona seguridad | 5.0 cm | Distancia segura desde límites |
| Settle time | 500 ms | Tiempo en tolerancia para confirmar llegada |
| PWM rango | [300, 700] | Rango del actuador |
| Anti-windup | ±500 | Saturación de integral |

---

## Algoritmo PID

### PID Ángulo (control principal)

#### Ecuación discreta

```
error(k) = normalizar(0° - ángulo_actual)       // [-180°, +180°]

integral(k) = integral(k-1) + error(k) × dt

derivada(k) = (error(k) - error(k-1)) / dt

derivada_filtrada(k) = α × derivada(k) + (1-α) × derivada_filtrada(k-1)

output(k) = Kp × error(k) + Ki × integral(k) + Kd × derivada_filtrada(k)
```

Donde:
- `dt` = período de muestreo (~1 ms a 1000 Hz)
- `α` = 0.2 (coeficiente de filtro pasa-bajos)

#### Anti-windup

La integral se satura para evitar acumulación excesiva:

```cpp
if (integral > INTEGRAL_MAX) integral = INTEGRAL_MAX;    // +500
if (integral < -INTEGRAL_MAX) integral = -INTEGRAL_MAX;  // -500
```

#### Aplicación al motor (con bias de zona muerta)

```cpp
if (|error| <= tolerancia) {
    Motor::freno();                    // Zona muerta angular
    outputSaturated = 0;
} else {
    pwm = |output| + MOTOR_BIAS;       // Bias = 300 (zona muerta del motor)
    pwm = min(pwm, pwm_max);           // Saturar al máximo
    if (output > 0) Motor::adelante(pwm);
    else            Motor::atras(pwm);
    outputSaturated = ±pwm;            // Con signo de dirección
}
```

> **Nota:** El `MOTOR_BIAS` (300 PWM) se suma a la salida PID para garantizar que el motor
> siempre supere la zona muerta mecánica. Esto reemplaza el anterior clamp `pwm_min` y
> proporciona una respuesta lineal: una salida PID de 1 produce PWM=301, de 100 produce PWM=400.
```

### Diagrama de bloques

```
                    ┌─────────────────────────────────────────────────┐
                    │         Controlador PID Ángulo                  │
                    │                                                 │
 Setpoint=0° ──┬──►(+)──► error ──┬──► Kp ──────────────────┐        │
               │   │              │                         │        │
               │  (-)◄───────────┼─────────────────────────┼─────┐  │
               │   │   normalizar │                         │     │  │
               │   │   ±180°      ├──► ∫ ──► Ki ────────────┤     │  │
               │   │              │         (anti-windup)   │     │  │
               │   │              │                         ▼     │  │
               │   │              └──► d/dt ──► Kd ──► Σ ──►PWM  │  │
               │   │                   (filtrado α=0.2)     │     │  │
               │   │                                        │     │  │
               │   └────────────────────────────────────────┼─────┘  │
               │                                            │        │
               │                                            ▼        │
               │                                         Motor DC    │
               │                                            │        │
               │                                            ▼        │
               └─────◄───── AS5600 ◄── Ángulo ◄────── Péndulo       │
                                                                     │
                    └─────────────────────────────────────────────────┘
```

### PID Posición interno (pre-posicionamiento y reposicionamiento)

Usado en estados `PIDA_PREPOSITIONING` y `PIDA_REPOSITIONING`. Misma ecuación discreta que el PID de ángulo, pero aplicada a la posición del carro:

```
error(k) = 45.0 cm - pos_actual_cm
```

Usa el sensor HC-020K IR para posición del carro. Se confirma la llegada cuando el error permanece dentro de tolerancia (1 cm) durante 500 ms.

---

## Rango de trabajo y convención angular

### Convención angular

| Ángulo | Posición del péndulo |
|--------|---------------------|
| 0° / 360° | Vertical arriba (equilibrio inestable — **setpoint**) |
| 180° | Colgando abajo (equilibrio estable) |

El sensor AS5600 proporciona 0°–360° con wrapping. El error se normaliza a **[-180°, +180°]** para manejar correctamente la transición 360°/0°:

```cpp
// Ejemplo: ángulo = 358° (péndulo ligeramente inclinado a la derecha)
error = 0° - 358° = -358°
normalizado = -358° + 360° = +2°      // Necesita moverse +2° hacia la derecha
```

### Rango de trabajo

El rango de trabajo define la banda angular desde 0° donde el PID de ángulo se activa/desactiva:

```
┌─────────────────────────────────────────────────────────────┐
│                    Espacio Angular (0°-360°)                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  360°-rango                    0°                  rango    │
│      │                         │                      │     │
│      ▼                         ▼                      ▼     │
│  ┌───┴─────────────────────────┴──────────────────────┴──┐  │
│  │     RANGO DE TRABAJO       0°        RANGO DE TRABAJO │  │
│  │   (350°..360°)         (vertical)    (0°..10°)        │  │
│  │      PID ACTIVO            ↑           PID ACTIVO     │  │
│  └───────────────────────────┬┬──────────────────────────┘  │
│                              ││                             │
│                   ┌──────────┘└──────────┐                  │
│                   │ FUERA DE RANGO       │                  │
│                   │ 10°...350°           │                  │
│                   │ → Freno + Reposicionar│                  │
│                   └─────────────────────┘                   │
│                                                             │
│  Con rango_trabajo = 10°:                                   │
│    Banda activa: [350°, 360°] ∪ [0°, 10°]                  │
│    Fuera de rango: (10°, 350°)                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Comportamiento:**
1. En estado `PIDA_READY`: si el ángulo entra en el rango de trabajo → auto-transiciona a `PIDA_RUNNING`
2. En estado `PIDA_RUNNING`: si el ángulo sale del rango de trabajo → freno + transiciona a `PIDA_REPOSITIONING`

### Zonas de seguridad (PID posición interno)

```
┌────────────────────────────────────────────────────────────────────┐
│                          Riel (90 cm)                              │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  LS_START                                              LS_END     │
│     │                                                      │       │
│     ▼                                                      ▼       │
│  ┌──┴───────┬──────────────────────────────────┬───────────┴──┐   │
│  │ PELIGRO  │         ZONA SEGURA              │   PELIGRO   │   │
│  │ (0-5cm)  │         (5-85 cm)                │  (85-90cm)  │   │
│  └──────────┴──────────────────────────────────┴──────────────┘   │
│                             ↑                                      │
│                          45 cm                                     │
│                    (setpoint pre-pos)                               │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

Protecciones durante pre-posicionamiento y reposicionamiento:
- Si el carro entra en zona de peligro Y se aleja del setpoint hacia el límite → freno → STOPPED
- Si un limit switch se activa en la dirección de movimiento → freno → STOPPED

---

## Telemetría

### Modo dual: Normal (5 Hz) y Fast (~14-17 Hz)

La telemetría usa dos modos según el estado del control:

| Modo | Función | Frecuencia | Campos | Tamaño ~aprox |
|------|---------|------------|--------|---------------|
| **Normal** | `publishTelemetry()` | 5 Hz (200ms) | Todos (~30 campos + pida_*) | ~600 bytes |
| **Fast** | `publishTelemetryFast()` | 14-17 Hz (50ms target) | 19 campos esenciales | ~170 bytes |

La selección es automática: cuando `PidAnguloMode::isActive()` es `true` y el modo activo es `MODE_PID_ANG`, se usa Fast.

```cpp
// En main.cpp — buildTelemetry()
pkt.telemetry_fast = (activeMode == MODE_PID_ANG) && PidAnguloMode::isActive();
```

### Campos de telemetría PID Ángulo (`pida_*`)

Estos campos se incluyen en `pendulum/telemetry` cuando el modo activo es `MODE_PID_ANG`:

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `pida_error` | float | Error angular normalizado (°, rango ±180) |
| `pida_integral` | float | Término integral acumulado |
| `pida_derivada` | float | Término derivativo filtrado |
| `pida_output` | float | Salida PID cruda (antes de saturación) |
| `pida_output_sat` | float | Salida PID saturada (PWM aplicado con signo) |
| `pida_setpoint` | float | Setpoint angular (siempre 0°) |
| `pida_kp` | float | Ganancia Kp actual |
| `pida_ki` | float | Ganancia Ki actual |
| `pida_kd` | float | Ganancia Kd actual |
| `pida_rango_trabajo` | float | Rango de trabajo configurado (°) |
| `pida_tolerancia` | float | Tolerancia configurada (°) |
| `pida_pwm_min` | int | PWM mínimo configurado |
| `pida_pwm_max` | int | PWM máximo configurado |
| `pida_pos_setpoint` | float | Setpoint de posición interna (45 cm) |

> **Nota:** Los campos `pida_kp`, `pida_ki`, `pida_kd`, `pida_rango_trabajo`, etc. son datos estáticos que no cambian durante la ejecución del PID. En modo Fast podrían omitirse en futuras optimizaciones.

### Campos compartidos con telemetría base

| Campo | Tipo | Relevancia para PID ángulo |
|-------|------|---------------------------|
| `angle_pend` | float | **Entrada principal** — ángulo del péndulo (°) |
| `pos_ir_cm` | float | Posición del carro — usado por PID posición interno |
| `pwm` | int | PWM aplicado al motor |
| `direction` | int | Dirección motor (1=ADL, -1=ATR, 0=DET) |
| `ol_state` | string | Estado del modo (`PIDA_RUNNING`, etc.) |
| `ls_start`, `ls_end` | bool | Estado de limit switches |

### Ejemplo de telemetría durante PIDA_RUNNING

```json
{
  "ts": 1743850000,
  "modo": "PID_ANG",
  "ol_state": "PIDA_RUNNING",
  "angle_pend": 358.5,
  "pos_ir_cm": 44.8,
  "pwm": 350,
  "direction": 1,
  "pida_error": 1.5,
  "pida_integral": 3.2,
  "pida_derivada": -0.8,
  "pida_output": 350.0,
  "pida_output_sat": 350.0,
  "pida_setpoint": 0.0,
  "pida_kp": 40.0,
  "pida_ki": 1.0,
  "pida_kd": 0.5,
  "pida_rango_trabajo": 20.0,
  "pida_tolerancia": 0.1,
  "pida_pwm_min": 250,
  "pida_pwm_max": 900,
  "pida_pos_setpoint": 45.0,
  "ls_start": false,
  "ls_end": false
}
```

---

## Comunicación MQTT

El sistema intercambia datos entre el firmware (ESP32) y la HMI (Backend + Frontend) a través de un broker MQTT (Mosquitto). Toda la telemetría, comandos y señales de estado viajan por este canal.

### Flujo de datos

```
┌────────────┐     ┌───────────┐     ┌────────────┐     ┌────────────┐
│   ESP32    │─MQTT─►           │─MQTT─►            │─ WS ─►            │
│  Firmware  │     │ Mosquitto │     │  Backend   │     │ Frontend   │
│  (SPISC)   │◄────│  :1883    │◄────│  :8000     │◄────│  :5173     │
└────────────┘     └───────────┘     └────────────┘     └────────────┘
     WiFi              TCP             HTTP/WS          Navegador
```

- **ESP32 → Backend:** El firmware publica telemetría y estado por MQTT. El backend (aiomqtt) recibe todos los tópicos con suscripción wildcard `pendulum/#` y reenvía los datos al frontend vía WebSocket.
- **Frontend → ESP32:** El usuario interactúa con la HMI en el navegador. Los comandos viajan por WebSocket al backend, que los traduce a un `publish` MQTT hacia el tópico correspondiente. El firmware los recibe en Core 1 y los encola en una cola FreeRTOS (máx. 10) para que Core 0 los procese.

### Tópicos de datos (ESP32 → Backend)

Estos tópicos son publicados por el firmware y recibidos por el backend.

| Tópico | Frecuencia | Payload aprox. | Contenido |
|--------|-----------|----------------|----------|
| `pendulum/telemetry` | **5 Hz** (normal) / **20 Hz** (PID activo) | ~500 B / ~150 B | Posición (IR+MAG), RPM, ángulo péndulo, vel. angular, PWM, dirección, IMU, limit switches, campos `pid_*` y `pida_*` |
| `pendulum/status` | **2 Hz** | ~200 B | Estado del sistema, modo activo, LS inicio/fin, WiFi RSSI, flags de calibración |
| `pendulum/limits` | **Evento** (solo al cambiar) | ~30 B | Estado de limit switches: `inicio`, `fin` (bool) |
| `pendulum/sweep/results` | **Evento** (fin de barrido) | Variable | Resultados del barrido PWM |

### Tópicos de comando (Backend → ESP32)

Estos tópicos son publicados por el backend cuando el usuario envía un comando desde la HMI.

| Tópico | Cuándo se envía | Payload principal |
|--------|----------------|-------------------|
| `pendulum/cmd/motor` | Control manual del motor | `action`, `pwm`, `direction` |
| `pendulum/cmd/mode` | Cambio de modo de operación | `mode` (MANUAL, PID_POS, PID_ANG, OPEN_LOOP, …) |
| `pendulum/cmd/calibrate` | Botón de calibración | `type` (angle, position, mpu, full) |
| `pendulum/cmd/config` | Ajuste de parámetro en tiempo real | `param`, `value` |
| `pendulum/cmd/stop` | Parada de emergencia (global) | `{}` (vacío) |
| `pendulum/cmd/sweep` | Control de barrido PWM | `action` (start, stop, reiniciar) |
| `pendulum/cmd/openloop` | Modo lazo abierto | `action`, `pwm`, `duration` |
| `pendulum/cmd/pidpos` | Modo PID posición | `action`, `kp/ki/kd`, `setpoint`, `sensor`, … |
| `pendulum/cmd/pidang` | Modo PID ángulo | `action`, `kp/ki/kd`, `rango_trabajo`, `tolerancia`, … |
| `pendulum/cmd/debug` | Diagnóstico (reservado) | — |
| `pendulum/time` | **Automático cada 10 s** | `ts` (epoch ms) — sincronización de reloj |

### Parámetros técnicos clave

| Parámetro | Valor | Qué significa |
|-----------|-------|---------------|
| **Buffer MQTT (PubSubClient)** | 1024 bytes | Tamaño máximo de un mensaje MQTT que el ESP32 puede enviar o recibir. Si un JSON supera este tamaño, se trunca y se pierde. |
| **Buffer JSON (serialización)** | 1280 bytes | Espacio reservado en memoria del ESP32 para armar el string JSON antes de publicarlo. Es mayor que el buffer MQTT para dejar margen de seguridad. |
| **QoS** | 0 (fire-and-forget) | Nivel de garantía de entrega MQTT. QoS 0 significa que el mensaje se envía una sola vez sin confirmación. Si se pierde por WiFi inestable, no se reintenta. Es el más rápido. |
| **Mensajes retenidos** | Ninguno | El broker no almacena el último mensaje de ningún tópico. Si un cliente se conecta tarde, no recibe datos anteriores. |
| **Keep-alive MQTT** | 5 s | Cada 5 segundos el ESP32 envía un ping al broker para confirmar que sigue vivo. Si el broker no recibe ping en 1.5× este valor (7.5 s), cierra la conexión. |
| **TCP NoDelay** | Activado | Desactiva el algoritmo de Nagle en el socket TCP. Sin esto, el sistema operativo esperaría ~40 ms acumulando datos antes de enviarlos, agregando latencia innecesaria. |
| **Safety timeout MQTT** | 10 s | Si el ESP32 no recibe **ningún** mensaje MQTT durante 10 segundos, frena el motor automáticamente como medida de seguridad. Evita que el motor siga girando si la conexión se pierde. |
| **Cola de comandos** | Máx. 10 | Los comandos recibidos por MQTT en Core 1 se encolan en una cola FreeRTOS. Core 0 los procesa uno por uno. Si llegan más de 10 sin procesar, los nuevos se descartan. |
| **Reconexión MQTT** | Backoff exponencial: 500 ms → 5 s | Si el ESP32 pierde conexión con el broker, reintenta conectarse esperando 500 ms, luego 1 s, 2 s, hasta un máximo de 5 s entre intentos. |
| **Ancho de banda pico** | ~22 msgs/s ≈ 9 KB/s | Durante PID activo (20 Hz telemetría + 2 Hz status) se generan ~22 mensajes por segundo, consumiendo aproximadamente 9 KB/s de ancho de banda WiFi. |

### Telemetría por estado

La frecuencia de telemetría depende del estado actual de la máquina de estados. Los estados con motor en movimiento usan telemetría rápida (20 Hz) y el resto usa normal (5 Hz).

| Estado | Motor activo | Telemetría | Frecuencia |
|--------|:------------:|-----------|:----------:|
| `PIDA_IDLE` | No | Normal | 5 Hz |
| `PIDA_CALIBRATING` | No | Normal | 5 Hz |
| `PIDA_HOMING_START` | Sí | Fast | 20 Hz |
| `PIDA_HOMING_END` | Sí | Fast | 20 Hz |
| `PIDA_PREPOSITIONING` | Sí | Fast | 20 Hz |
| `PIDA_READY` | No | Normal | 5 Hz |
| `PIDA_RUNNING` | Sí | Fast | 20 Hz |
| `PIDA_REPOSITIONING` | Sí | Fast | 20 Hz |
| `PIDA_STOPPED` | No | Normal | 5 Hz |

La selección se hace en `main.cpp` con la función `PidAnguloMode::isActive()`, que retorna `true` para los 5 estados con motor activo.

---

## Guía de uso

### Flujo típico de operación

```
1. Activar modo PID Ángulo
   └─► switchMode(MODE_PID_ANG)
       └─► begin() → Auto-homing si no está en LS_START

2. Configurar parámetros PID
   └─► Enviar comando config con Kp, Ki, Kd, rango_trabajo, tolerancia
       └─► Estado: PIDA_READY o PIDA_IDLE (configurado)

3. Ejecutar secuencia
   └─► Enviar comando run
       └─► Homing a LS_START
           └─► Calibración (200ms)
               └─► Pre-posicionamiento a 45 cm
                   └─► Estado: PIDA_READY

4. Levantar péndulo a vertical
   └─► Ángulo entra en rango de trabajo (ej: 358°–360° ó 0°–10°)
       └─► Auto-transición a PIDA_RUNNING
           └─► PID ángulo activo a 1000 Hz

5. Control activo
   └─► Motor ajusta posición del carro para mantener péndulo vertical
       └─► Si péndulo cae (sale del rango):
           └─► Freno → Reposicionamiento a 45 cm → READY
               └─► Volver a levantar el péndulo → paso 4

6. Detener
   └─► Enviar comando stop
       └─► Estado: PIDA_STOPPED
```

### Consideraciones importantes

1. **Siempre ejecutar `run` primero**: El comando `run` inicia la secuencia completa (homing → calibración → pre-posicionamiento → ready). Sin esto, el carro puede no estar centrado.

2. **El setpoint es siempre 0°**: A diferencia del PID de posición, no hay setpoint configurable. El objetivo es mantener el péndulo en vertical.

3. **La transición a RUNNING es automática**: No se requiere un segundo comando. Basta con levantar el péndulo a la zona vertical.

4. **Monitorear telemetría**: Observar `pida_error` y `pida_output` para ajustar ganancias.

5. **Ajuste de ganancias**:
   - Si oscila rápido: ↓ Kp, ↑ Kd
   - Si la respuesta es lenta: ↑ Kp
   - Si hay desplazamiento constante: ↑ Ki (con cuidado — alto Ki causa inestabilidad)
   - Si vibra/tiembla: ↑ Tolerancia (zona muerta más amplia)

---

## Ejemplos de comandos

### Secuencia básica completa

```bash
# 1. Activar modo PID Ángulo
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/mode" \
  -m '{"mode":"PID_ANG"}'

# 2. Configurar parámetros PID
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidang" \
  -m '{"action":"config","kp":40.0,"ki":1.0,"kd":0.5,"rango_trabajo":20,"tolerancia":0.1}'

# 3. Iniciar secuencia completa (homing → calibración → pre-pos → ready)
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidang" \
  -m '{"action":"run"}'

# 4. [Levantar el péndulo a la vertical → PID se activa automáticamente]

# 5. Detener si es necesario
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidang" \
  -m '{"action":"stop"}'

# 6. Volver a intentar (home + run)
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidang" \
  -m '{"action":"run"}'
```

### Comandos rápidos

```bash
# Ir a home
mosquitto_pub -t "pendulum/cmd/pidang" -m '{"action":"home"}'

# Ir a end
mosquitto_pub -t "pendulum/cmd/pidang" -m '{"action":"end"}'

# Stop de emergencia (global)
mosquitto_pub -t "pendulum/cmd/stop" -m '{}'

# Solo cambiar ganancias (mantener otros parámetros)
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","kp":5.0,"kd":0.8}'

# Configurar rango de trabajo más amplio
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","rango_trabajo":20}'

# Reset a IDLE
mosquitto_pub -t "pendulum/cmd/pidang" -m '{"action":"reset"}'
```

### Monitoreo de telemetría

```bash
# Suscribirse a telemetría completa
mosquitto_sub -h 192.168.4.2 -t "pendulum/telemetry" -v

# Filtrar solo campos PID ángulo (con jq)
mosquitto_sub -h 192.168.4.2 -t "pendulum/telemetry" | \
  jq '{state: .ol_state, angle: .angle_pend, error: .pida_error, output: .pida_output_sat}'

# Monitorear estado + posición del carro
mosquitto_sub -h 192.168.4.2 -t "pendulum/telemetry" | \
  jq '{state: .ol_state, pos: .pos_ir_cm, angle: .angle_pend}'
```

---

## Troubleshooting

### Problema: El péndulo no se estabiliza (oscila y cae)

**Posibles causas:**
1. Kp muy alto → oscilación violenta
2. Kd muy bajo → sin amortiguamiento
3. Rango de trabajo muy estrecho → PID se desactiva con pequeña desviación

**Soluciones:**
```bash
# Reducir Kp, aumentar Kd
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","kp":1.5,"kd":0.8}'

# Ampliar rango de trabajo para dar más margen
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","rango_trabajo":20}'
```

### Problema: El carro se desplaza constantemente hacia un lado

**Posibles causas:**
1. Ki muy bajo → no corrige error estacionario
2. Asimetría mecánica (rozamiento diferente en cada dirección)
3. Pre-posicionamiento lejos del centro

**Soluciones:**
```bash
# Aumentar Ki gradualmente
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","ki":0.2}'
```

### Problema: El carro vibra/tiembla sin moverse significativamente

**Posibles causas:**
1. Tolerancia muy baja → el PID intenta corregir micro-errores
2. PWM mínimo muy alto → saltos bruscos de potencia
3. Kp alto con Kd bajo

**Soluciones:**
```bash
# Aumentar tolerancia (zona muerta)
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","tolerancia":3.0}'

# Reducir PWM mínimo
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","pwm_min":150}'
```

### Problema: El PID no se activa (queda en READY)

**Posibles causas:**
1. El péndulo no alcanza el rango de trabajo
2. Rango de trabajo configurado muy estrecho
3. Sensor AS5600 no reporta ángulo correcto

**Soluciones:**
```bash
# Ampliar rango de trabajo
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","rango_trabajo":25}'

# Verificar ángulo en telemetría
mosquitto_sub -t "pendulum/telemetry" | jq '.angle_pend'
```

### Problema: Timeout de homing

**Posibles causas:**
1. Limit switch no funciona o desconectado
2. Motor no tiene suficiente torque con HOMING_PWM (300)
3. Obstrucción mecánica

**Soluciones:**
1. Verificar conexión de limit switches con `test_limit_switches.ino`
2. Verificar que el HOMING_PWM (300) es suficiente para mover el carro
3. Revisar mecánica del sistema

### Problema: El carro choca con los límites durante control angular

**Posibles causas:**
1. Péndulo cae y el PID intenta corregir con PWM alto
2. Ki acumulada excesiva empuja el carro

**Soluciones:**
- El sistema detecta automáticamente los limit switches y transiciona a REPOSITIONING
- Reducir pwm_max para limitar la velocidad máxima del carro
- Reducir Ki para evitar acumulación excesiva

```bash
mosquitto_pub -t "pendulum/cmd/pidang" \
  -m '{"action":"config","pwm_max":500,"ki":0.02}'
```

---

## Protecciones de seguridad

El modo PID de ángulo incluye 6 capas de protección:

| # | Protección | Activación | Acción |
|---|-----------|------------|--------|
| 1 | **Limit switches** | LS activo en dirección de movimiento | Freno → REPOSITIONING o STOPPED |
| 2 | **Zona de peligro** | pos < 5cm o pos > 85cm + movimiento hacia límite | Freno → STOPPED |
| 3 | **Timeout homing** | 30s sin alcanzar limit switch | Freno → STOPPED |
| 4 | **Rango de trabajo** | Ángulo fuera de banda configurable | Freno → REPOSITIONING → READY |
| 5 | **Parada de emergencia** | `pendulum/cmd/stop` (global) | Freno inmediato → STOPPED |
| 6 | **Anti-windup** | Integral PID (ángulo y posición) | Clamp a ±500 |

---

## Archivos del sistema

### Firmware (SPISC/)

| Archivo | Descripción |
|---------|-------------|
| `include/telemetry_data.h` | Enum `MODE_PID_ANG` + 15 campos `pida_*` en `TelemetryPacket` |
| `include/wifi_mqtt_config.h` | `TOPIC_CMD_PID_ANG "pendulum/cmd/pidang"` |
| `src/modes/pid_angulo_mode.h` | Header — interfaz del namespace (17 funciones públicas) |
| `src/modes/pid_angulo_mode.cpp` | Implementación completa (~530 líneas): máquina de estados, PID ángulo, PID posición interno |
| `src/main.cpp` | 7 puntos de integración (include, switchMode, stateStr, routing ×3, buildTelemetry, taskLoop) |
| `src/mqtt_comm.cpp` | Suscripción a `pendulum/cmd/pidang` + serialización JSON de campos `pida_*` |

### Backend (hmi/backend/)

| Archivo | Descripción |
|---------|-------------|
| `config.py` | `TOPICO_CMD_PID_ANG = "pendulum/cmd/pidang"` |
| `modelos.py` | Estados `PIDA_*`, modelo `ComandoPidAng`, campos `pida_*` en `Telemetria` |
| `websocket_manager.py` | Ruta `cmd_pidang` → publica en `TOPICO_CMD_PID_ANG` |

### Frontend (hmi/frontend/src/)

| Archivo | Descripción |
|---------|-------------|
| `stores/pendulumStore.js` | Estado `pidAngState`, resultados `pidAngResults`, 7 acciones, normalización de campos `pida_*` |
| `components/tabs/TabControlPidAngulo.vue` | Panel de control: parámetros PID, botones de acción, indicadores |
| `components/ResultadosPidAngulo.vue` | Tabla de resultados + exportación CSV con prefijo `PIDA` |
| `components/PaginaInicio.vue` | Tarjeta de navegación con icono ⚖ |
| `App.vue` | Ruta `pid_angulo` → `TabControlPidAngulo` |

---

## Estado actual del sistema

### Qué funciona (validado en hardware — Test 2, 05/04/2026)

- Máquina de estados completa con 9 estados y transiciones
- PID de ángulo a 1000 Hz con normalización angular ±180°
- PID de posición interno para pre-posicionamiento a 45 cm usando sensor IR (corregido de MAG en v1.1)
- Auto-detección de entrada/salida del rango de trabajo
- Reposicionamiento automático del carro al salir del rango (~1-2 s)
- Homing automático a LS_START y LS_END
- Telemetría: 15 campos `pida_*` + 4 campos `pida_pos_*` (error, integral, derivada, output del PID posición interno)
- Telemetría fast incluye campos de configuración (Kp, Ki, Kd, rango, tolerancia, PWM min/max)
- Telemetría dinámica (fast durante PID activo, normal en reposo)
- HMI completo: panel de control, indicadores, 6 gráficos por defecto, resultados con columnas de PID posición, exportación CSV
- Debug prints con rate-limiting (~200 ms) en estados PREPOSITIONING y REPOSITIONING
- Pre-posicionamiento rápido: **~5-6 s** (vs ~65 s en Test 1 con sensor MAG incorrecto)
- Configuración dinámica de ganancias vía HMI durante operación
- Compilación exitosa: RAM 24.8%, Flash 64.1%

### Resultados de pruebas hardware

#### Test 1 (05/04/2026 — pre-fix)

Primer test con firmware sin las correcciones. Problemas detectados:
- Sensor de posición incorrecto (MAG en vez de IR) → pre-posicionamiento tardaba ~65 s
- Campos de configuración PID no llegaban en telemetría fast (todos en 0)
- Sin gráficos `pida_*` en HMI (no definidos en historial ni en GraficosTelemetria)
- graficosDefault referenciaban IDs inexistentes
- Sin columnas de PID posición interno en tabla de resultados
- Sin debug prints para diagnóstico del PID posición interno

#### Test 2 (05/04/2026 — post-fix, v1.1)

Datos: `datas/pid_angulo_2026-04-05T22-29-04.csv` (2324 muestras, 35 columnas)  
Log serial: `logs/log_pid_angulo_2026-04-05T22-29-04.md`

**Distribución de estados:** RUNNING=1593, REPOSITIONING=510, HOMING=128, PREPOSITIONING=93

Se probaron 5 configuraciones durante la sesión:

| Config | Kp | Ki | Kd | RT | Muestras RUNNING | Runs | Max run sostenido | Err áng. medio | Err áng. max | Err pos. medio | Salidas a repos |
|--------|----|----|----|----|-----------------|------|-------------------|----------------|-------------|----------------|------------------|
| 1 | 40.0 | 3.26 | 0.68 | 15° | 228 | 2 | 222 (~12 s) | 2.48° | 14.8° | 4.45 cm | 2 |
| 2 | 30.2 | 0.0 | 0.0 | 20° | 190 | 2 | 183 (~10 s) | 2.99° | 20.0° | 4.71 cm | 2 |
| 3 | 50.0 | 0.0 | 0.0 | 20° | 400 | 6 | 388 (~34 s) | 1.85° | 19.9° | 8.29 cm | 6 |
| 4 | 50.0 | 5.0 | 0.0 | 20° | 156 | 21 | 67 (~6 s) | 6.78° | 20.0° | 7.51 cm | 21 |
| **5** | **50.0** | **5.0** | **0.0** | **10°** | **619** | **1** | **619 (~3 min 12 s)** | **1.16°** | **9.9°** | **5.47 cm** | **0** |

**Mejor configuración encontrada: Kp=50, Ki=5, RT=10°** — mantuvo el péndulo estabilizado durante **3 min 12 s** continuos sin ninguna salida a repositioning.

**Observaciones clave:**
- El rango de trabajo estrecho (RT=10°) es más selectivo: solo activa el PID cuando el péndulo está realmente cerca de la vertical, evitando activaciones espurias que desestabilizan
- Ki=5 con RT=20° fue la peor config (21 salidas en 156 muestras) — la integral se acumula demasiado con rango amplio
- Ki=5 con RT=10° fue la mejor — la integral ayuda a corregir error estacionario sin acumularse en exceso
- Durante RUNNING, el carro se desplaza entre 26 y 70 cm (span de ~44 cm), lo que indica que aún no hay control de posición activo durante la estabilización angular
- El carro alcanzó ambos limit switches (LS_START y LS_END) durante fases de control agresivo
- El timeout de seguridad MQTT se disparó una vez (22:22:15–22:22:38) — problema intermitente de WiFi

### Limitaciones conocidas

- **Desplazamiento del carro durante RUNNING**: El PID de ángulo mueve el carro sin restricción de posición. En la mejor sesión (3 min 12 s), el carro se desplazó entre 26–70 cm. Solución propuesta: control en cascada ángulo→posición.
- **Timeout de seguridad MQTT**: Se dispara intermitentemente (~1 vez por sesión de prueba), causando freno forzado y pérdida de la estabilización. Posible causa: latencia WiFi AP del ESP32.
- **Ganancias por calibrar**: Los valores Kp=50, Ki=5 son un buen punto de partida pero requieren más iteración, especialmente agregar Kd para amortiguamiento.
- **Sin Kd activo en mejor config**: Todas las configs probadas con Kd=0. Agregar derivativo podría mejorar la amortiguación de oscilaciones.

---

## Mejoras futuras propuestas

### 1. Ajuste fino de ganancias con Kd (Prioridad: Alta)

La mejor configuración encontrada (Kp=50, Ki=5, RT=10°) no usa término derivativo (Kd=0). Agregar Kd debería mejorar la amortiguación de oscilaciones y reducir el error angular promedio. Procedimiento sugerido:

1. Partir de Kp=50, Ki=5, Kd=0, RT=10° (mejor config del Test 2)
2. Subir Kd gradualmente: 0.5 → 1.0 → 2.0
3. Observar si el error angular promedio baja de 1.16° y si las oscilaciones son más suaves
4. Si aparece vibración, reducir Kd

### 2. Control en cascada ángulo → posición (Prioridad: Alta)

El carro se desplaza libremente durante RUNNING (26–70 cm en el Test 2). Implementar un PID en cascada: el PID de ángulo genera un sesgo de posición, y un PID de posición subyacente mantiene el carro cerca del centro. Esto evitaría los impactos contra limit switches y extendería la duración del control.

### 3. Mitigar timeout MQTT de seguridad (Prioridad: Alta)

El timeout MQTT (10 s) se dispara intermitentemente (~1 vez por sesión) durante el control activo, frenando el motor y desestabilizando el péndulo. Opciones:
- Aumentar timeout a 15-20 s durante modos de control activo
- Implementar heartbeat MQTT desde el HMI con mayor frecuencia
- Desactivar timeout durante las fases RUNNING con watchdog interno alternativo

### 4. Swing-up automático (Prioridad: Media)

Implementar algoritmo de energía para llevar el péndulo de 180° a 0° automáticamente, eliminando la necesidad de levantar manualmente. Transición suave a PID de ángulo cuando se alcanza el rango de trabajo.

### 5. Fusión de sensores para ángulo (Prioridad: Baja)

Combinar AS5600 (posición angular) con MPU6050 (velocidad angular via giroscopio) usando un filtro complementario para mejorar la estimación del ángulo y velocidad angular.

### 6. Logging de intentos de estabilización (Prioridad: Baja)

Registrar cada intento de estabilización (duración, error máximo, causa de caída) para analizar el rendimiento del controlador y optimizar ganancias.

---

## Referencias

- [Control PID de Posición](control_pid_posicion.md) — Modo de control de posición del carro (referencia de implementación)
- [Modo Lazo Abierto](../SPISC/src/modes/lazo_abierto_mode.cpp) — Lógica de homing reutilizada
- [Calibración Sensor Magnético](calibracion_posicion_mag.md) — Calibración del AS5600
- [Arquitectura Dual-Core](config_dual_core.md) — Ejecución en Core 0/1
- [Plan de Mejoras](plan_mejoras.md) — Mejoras globales del sistema
