# Control PID Cascada — Péndulo Invertido

**Proyecto:** [SPISC — Sistema de Péndulo Invertido con Supervisión y Control](../README.md)  
**Fecha:** 06/04/2026  
**Última actualización:** 06/04/2026 (v1.1 — PREPOSITIONING + auto-transición + bloqueo direccional + tolerancia posición)  
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
- [Protecciones de seguridad](#protecciones-de-seguridad)
- [Estado actual del sistema](#estado-actual-del-sistema)
- [Mejoras futuras propuestas](#mejoras-futuras-propuestas)
- [Referencias](#referencias)

---

## Descripción general

El modo **Control PID Cascada** (`MODE_PID_CASC`) implementa un controlador cascada con dos lazos PID encadenados para estabilizar el péndulo invertido en posición vertical mientras controla la posición del carro. Este modo:

- Combina un lazo externo de posición (genera θ_ref) con un lazo interno de ángulo (genera PWM)
- Incluye pre-posicionamiento automático del carro al centro del riel antes de habilitar el control
- Detecta automáticamente la entrada del péndulo al rango de trabajo y auto-transiciona a control activo
- Maneja la transición angular 360°/0° mediante normalización del error a [-180°, +180°]
- Usa bloqueo direccional de límites (solo frena si el motor empuja hacia el límite activo)
- Expone telemetría extendida con campos `pidc_*` para ambos lazos
- Permite configuración dinámica de ganancias y parámetros de ambos lazos vía MQTT

### Principio de funcionamiento

```
┌────────────────────────────────────────────────────────────────────────┐
│  Flujo de Control PID Cascada                                          │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  pos_setpoint ──┬──► (+) ──► Controlador  ──► θ_ref ──┬──► (+) ──► Controlador  ──► Motor │
│  (45 cm)        │     │      PID Posición    (±2°)     │     │      PID Ángulo        PWM  │
│                 │   (-)      (lazo externo)            │   (-)      (lazo interno)         │
│                 │     │                                │     │                              │
│  HC-020K ───────┴─────┘                    AS5600 ─────┴─────┘                              │
│  (posición cm)                             (ángulo 0°-360°, normalizado a ±180°)            │
│                                                                        │
│  Nota: El signo de θ_ref está invertido respecto al error de posición  │
│  para que el carro persiga la caída del péndulo en la dirección        │
│  correcta.                                                             │
│                                                                        │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ PID Posición para pre-posicionamiento (parámetros fijos)        │  │
│  │   Kp=6.0, Ki=1.5, Kd=0.0 — Setpoint = pos_setpoint (45 cm)    │  │
│  │   Solo activo en estado PIDC_PREPOSITIONING                     │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

### Características principales

| Característica | Valor por defecto | Rango configurable |
|---------------|-------------------|---------------------|
| **Frecuencia de control** | 1000 Hz (Core 0) | Fijo |
| **Sensor de ángulo** | AS5600 magnético | Fijo |
| **Sensor de posición** | HC-020K IR | Fijo |
| **Rango angular** | 0° - 360° | Fijo |
| **Setpoint angular** | Variable (θ_ref del lazo externo) | Indirecto |
| **Setpoint posición** | 45 cm (centro riel) | [5, 85] cm |
| **Rango de trabajo** | ±20° | [5°, 30°] |
| **Tolerancia angular** | 0.1° | [0°, 5°] |
| **Tolerancia posición** | 0.5 cm | [0, 5] cm |
| **θ_ref máximo** | ±2° | [0.5°, 10°] |
| **PWM mínimo** | 250 | [100, 500] |
| **PWM máximo** | 900 | [300, 1023] |
| **Anti-windup** | Saturación de integral ±500 | Fijo |
| **Filtro derivativo** | Pasa-bajos α = 0.2 | Fijo |
| **Motor bias** | 300 PWM (zona muerta) | Fijo |
| **Pos. pre-posicionamiento** | 45 cm (= pos_setpoint) | Configurable |
| **Telemetría (PID activo)** | ~14-17 Hz (fast) | Fijo 50ms target |
| **Telemetría (reposo)** | 5 Hz (normal) | Fijo 200ms |

---

## Arquitectura del modo

### Archivos involucrados

```
SPISC/
├── src/modes/
│   ├── pid_cascada_mode.h      ← Header con interfaz pública (26 getters)
│   └── pid_cascada_mode.cpp    ← Implementación del modo (~850 líneas)
├── include/
│   ├── telemetry_data.h        ← Enum MODE_PID_CASC + 23 campos pidc_* en TelemetryPacket
│   ├── hardware_pins.h         ← MOTOR_BIAS, TRACK_LENGTH_CM, HOMING_PWM
│   └── wifi_mqtt_config.h      ← TOPIC_CMD_PID_CASC "pendulum/cmd/pidcasc"
├── src/
│   ├── main.cpp                ← Orquestador dual-core (7 puntos de integración)
│   ├── mqtt_comm.cpp           ← Suscripción + serialización JSON pidc_*
│   └── encoder_ir.cpp          ← Posición con dirección motor/magnético
└── lib/
    └── MqttBridge/             ← Conexión WiFi+MQTT (buffer 2048 bytes)
```

### Integración en main.cpp (7 puntos)

1. **Include**: `#include "modes/pid_cascada_mode.h"`
2. **switchMode()**: Case `MODE_PID_CASC` → `PidCascadaMode::begin()`
3. **getStateStr()**: Case `MODE_PID_CASC` → `PidCascadaMode::getStateStr()`
4. **Command routing (cmd/pidcasc)**: → `PidCascadaMode::onCommand()`
5. **Command routing (cmd/stop)**: → `PidCascadaMode::onCommand()`
6. **buildTelemetry()**: Llena 23 campos `pidc_*` con getters + setea `telemetry_fast`
7. **taskControl loop**: Case `MODE_PID_CASC` → `PidCascadaMode::loop()`

### Namespace

```cpp
namespace PidCascadaMode {
    void begin();               // Inicializa modo (auto-homing)
    void loop();                // Loop de control (1000 Hz en Core 0)
    void onCommand(...);        // Procesa comandos MQTT
    const char* getStateStr();  // Estado como string
    bool isActive();            // ¿Motor en movimiento?
    
    // Getters telemetría — Lazo externo (Posición) — 10 campos
    float getPosError();        // Error de posición (cm)
    float getPosIntegral();     // Integral acumulada
    float getPosDerivada();     // Derivada filtrada
    float getPosOutput();       // Salida PID cruda (θ_ref antes de saturación)
    float getPosOutputSaturated(); // θ_ref saturado (±theta_ref_max)
    float getPosSetpoint();     // Setpoint posición (cm)
    float getPosKp(), getPosKi(), getPosKd();
    float getPosZonaSeguridad(); // Zona de seguridad (cm)

    // Getters telemetría — Lazo interno (Ángulo) — 10 campos
    float getAngError();        // Error angular normalizado (±180°)
    float getAngIntegral();     // Integral acumulada
    float getAngDerivada();     // Derivada filtrada
    float getAngOutput();       // Salida PID cruda
    float getAngOutputSaturated(); // PWM aplicado (con signo)
    float getAngSetpoint();     // θ_ref (variable, del lazo externo)
    float getAngKp(), getAngKi(), getAngKd();

    // Getters telemetría — Parámetros comunes — 6 campos
    float getRangoTrabajo();
    float getToleranciaAng();
    float getToleranciaPos();
    int   getPwmMin(), getPwmMax();
    float getThetaRefMax();
}
```

---

## Máquina de estados

```
┌──────────────────────────────────────────────────────────────────────┐
│                    Diagrama de Estados PID_CASC                      │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌──────────┐  cmd:run    ┌─────────────────┐                      │
│   │PIDC_IDLE │────────────►│PIDC_HOMING_START│                      │
│   └──────────┘  (auto)     └────────┬────────┘                      │
│        │                            │                                │
│        │ ya en home                 │ LS_START activo                │
│        │                            ▼                                │
│        │                   ┌────────────────┐                        │
│        │                   │PIDC_CALIBRATING│  (200ms pausa)         │
│        │                   └───────┬────────┘                        │
│        │                           │                                 │
│        │                           ▼                                 │
│        │                  ┌────────────────┐                         │
│        │                  │PIDC_WAIT_READY │  (2s estabilización)    │
│        │                  └───────┬────────┘                         │
│        │                          │                                  │
│        │                          ▼                                  │
│        │                ┌───────────────────┐                        │
│        │                │PIDC_PREPOSITIONING│  PID pos → 45 cm       │
│        │                └────────┬──────────┘                        │
│        │                         │                                   │
│        │          pos OK (500ms settle)                               │
│        │                         │                                   │
│        │                         ▼                                   │
│        └───────────────────►┌───────────┐                            │
│                             │PIDC_READY │  Auto-transición           │
│   ┌──────────────┐          └─────┬─────┘  cuando péndulo            │
│   │PIDC_HOMING   │◄──────────────┤         entra en rango            │
│   │    _END      │               │                                   │
│   └──────┬───────┘               │ ángulo entra                     │
│          │                       │ en rango de trabajo               │
│          │ LS_END activo         ▼                                   │
│          └──────────────►┌──────────────┐                            │
│                          │PIDC_RUNNING  │  Control cascada 1000Hz    │
│                          └──────┬───────┘                            │
│                                 │                                    │
│          ┌──────────────────────┼──────────────────┐                 │
│          │                      │                  │                 │
│          ▼                      ▼                  ▼                 │
│   ┌─────────────┐    ┌──────────────────┐  ┌──────────────────┐     │
│   │PIDC_STOPPED │    │péndulo fuera de  │  │límite físico     │     │
│   │  (cmd:stop) │    │rango de trabajo  │  │(direccional)     │     │
│   └──────┬──────┘    └───────┬──────────┘  └──────┬───────────┘     │
│          │                   │                    │                  │
│          │                   └────────────────────┘                  │
│          │                            │                              │
│          │                            ▼                              │
│          │                     PIDC_STOPPED                          │
│          │                                                           │
│          └──► cmd:reset → PIDC_IDLE                                  │
│              cmd:config → actualiza parámetros                       │
│              cmd:home/end → posicionamiento manual                   │
│              cmd:run → reinicia secuencia completa                   │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

### Descripción de estados

| Estado | Descripción |
|--------|-------------|
| `PIDC_IDLE` | Estado inicial. Auto-inicia homing si no está en LS_START. Acepta `config`, `home`, `end`, `run`. |
| `PIDC_HOMING_START` | Motor retrocediendo (PWM 300) hasta LS_START. Timeout: 30s. |
| `PIDC_HOMING_END` | Motor avanzando (PWM 300) hasta LS_END. Timeout: 30s. |
| `PIDC_CALIBRATING` | Pausa breve (200ms) para estabilización de sensores post-homing. Auto-calibración de posición a 0 cm via main.cpp. |
| `PIDC_WAIT_READY` | Espera de 2 segundos para estabilización mecánica y de sensores. |
| `PIDC_PREPOSITIONING` | PID de posición interno (Kp=6, Ki=1.5) mueve carro a `pos_setpoint` (45 cm). Tolerancia 0.5 cm, settle time 500ms. Protección direccional de límites. |
| `PIDC_READY` | Esperando que el péndulo entre al rango de trabajo. **Auto-transiciona a RUNNING** cuando el ángulo del péndulo entra en la banda definida por `rango_trabajo`. No requiere botón EJECUTAR. |
| `PIDC_RUNNING` | Control cascada activo a 1000 Hz. Ambos lazos ejecutándose. Telemetría rápida activada. Si el péndulo sale del rango de trabajo o un límite se activa en dirección de empuje → STOPPED. |
| `PIDC_STOPPED` | Detenido por comando `stop`, límite físico, o salida del rango de trabajo. Acepta `config`, `home`, `end`, `run`, `reset`. |

### Diferencias con PID Ángulo

| Aspecto | PID Ángulo (`pidang`) | PID Cascada (`pidcasc`) |
|---------|----------------------|------------------------|
| **Lazos de control** | 1 (ángulo) | 2 (posición → ángulo) |
| **Setpoint angular** | Fijo 0° (vertical) | Variable θ_ref (del lazo externo, ±2°) |
| **Control de posición** | Solo pre-pos/repos (fijo 45cm) | Activo durante RUNNING (setpoint configurable) |
| **Auto-transición READY→RUNNING** | Sí | Sí |
| **Reposicionamiento automático** | Sí (REPOSITIONING → READY) | No (→ STOPPED) |
| **Protección de límites** | Direccional (→ REPOSITIONING) | Direccional (→ STOPPED) |
| **Tolerancia posición** | No | Sí (zona muerta configurable) |
| **Sensor de control** | AS5600 (ángulo) | AS5600 (ángulo) + HC-020K (posición) |

### Flujo de la secuencia `run`

```
cmd:run → HOMING_START → LS_START → CALIBRATING (200ms)
       → WAIT_READY (2s estabilización)
       → PREPOSITIONING (PID pos → 45cm, settle 500ms)
       → READY (esperando péndulo vertical — auto-transición)
       → RUNNING (control cascada activo 1000 Hz)
       → STOPPED (si péndulo cae, límite activo, o cmd:stop)
```

---

## Comandos MQTT

**Tópico:** `pendulum/cmd/pidcasc`

### Tabla de comandos

| Acción | Payload JSON | Desde estados | Descripción |
|--------|--------------|---------------|-------------|
| `home` | `{"action":"home"}` | IDLE, READY, STOPPED | Mover a LS_START (home) |
| `end` | `{"action":"end"}` | IDLE, READY, STOPPED | Mover a LS_END |
| `config` | Ver debajo | IDLE, READY, STOPPED | Configurar parámetros PID |
| `run` | `{"action":"run"}` | IDLE, READY, STOPPED | Iniciar secuencia completa (homing→calib→wait→prepos→ready) |
| `stop` | `{"action":"stop"}` | Cualquiera | Detener inmediatamente (freno + reset variables PID) |
| `reset` | `{"action":"reset"}` | Cualquiera | Volver a PIDC_IDLE |

> **Nota:** El tópico global `pendulum/cmd/stop` también detiene este modo inmediatamente.

### Payload de configuración

```json
{
  "action": "config",
  "kp_pos": 0.5,
  "ki_pos": 0.01,
  "kd_pos": 0.1,
  "pos_setpoint": 45,
  "zona_seguridad": 5,
  "theta_ref_max": 2,
  "tolerancia_pos": 0.5,
  "kp_ang": 40,
  "ki_ang": 1,
  "kd_ang": 0.5,
  "rango_trabajo": 20,
  "tolerancia_ang": 0.1,
  "pwm_min": 250,
  "pwm_max": 900
}
```

> **Nota:** Todos los parámetros son opcionales. Solo se actualizan los incluidos en el payload. Los parámetros se validan y clampean a sus rangos válidos.

---

## Parámetros configurables

### Lazo externo (Posición → θ_ref)

| Parámetro | Default | Rango | Unidad | Descripción |
|-----------|---------|-------|--------|-------------|
| `kp_pos` | 0.5 | [0.0, 20.0] | - | Ganancia proporcional posición |
| `ki_pos` | 0.01 | [0.0, 5.0] | - | Ganancia integral posición |
| `kd_pos` | 0.1 | [0.0, 5.0] | - | Ganancia derivativa posición |
| `pos_setpoint` | 45.0 | [zona_seg, 90-zona_seg] | cm | Posición objetivo del carro en el riel |
| `zona_seguridad` | 5.0 | [2.0, 20.0] | cm | Margen de seguridad desde los extremos del riel |
| `theta_ref_max` | 2.0 | [0.5, 10.0] | ° | Saturación de la referencia angular (±grados) |
| `tolerancia_pos` | 0.5 | [0.0, 5.0] | cm | Zona muerta de posición: si \|error_pos\| ≤ tolerancia → θ_ref = 0 |

### Lazo interno (Ángulo → PWM)

| Parámetro | Default | Rango | Unidad | Descripción |
|-----------|---------|-------|--------|-------------|
| `kp_ang` | 40.0 | [0.1, 100.0] | - | Ganancia proporcional ángulo |
| `ki_ang` | 1.0 | [0.0, 50.0] | - | Ganancia integral ángulo |
| `kd_ang` | 0.5 | [0.0, 10.0] | - | Ganancia derivativa ángulo |
| `rango_trabajo` | 20.0 | [5.0, 30.0] | ° | Banda angular desde 0° para activar/desactivar PID |
| `tolerancia_ang` | 0.1 | [0.0, 5.0] | ° | Zona muerta angular: si \|error_ang\| ≤ tolerancia → freno motor |

### Motor

| Parámetro | Default | Rango | Unidad | Descripción |
|-----------|---------|-------|--------|-------------|
| `pwm_min` | 250 | [100, 500] | - | PWM mínimo (no usado directamente — el bias de 300 prevalece) |
| `pwm_max` | 900 | [300, 1023] | - | PWM máximo permitido |

### Validación de parámetros

Los parámetros se validan al recibirse por MQTT:
- Se clampean al rango válido (no se rechazan)
- El `pos_setpoint` se fuerza dentro de `[zona_seguridad, 90 - zona_seguridad]`
- Si `pwm_min >= pwm_max`, ambos se resetean a valores por defecto (250, 900)
- Los parámetros se aplican inmediatamente solo en estados no-activos (IDLE, READY, STOPPED)

### Parámetros fijos del PID posición de pre-posicionamiento

El PID de posición para pre-posicionamiento usa parámetros fijos no configurables vía MQTT:

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| Kp | 6.0 | Ganancia proporcional |
| Ki | 1.5 | Ganancia integral |
| Kd | 0.0 | Ganancia derivativa |
| Setpoint | `pos_setpoint` (45 cm default) | Centro del riel (configurable vía lazo externo) |
| Tolerancia | 0.5 cm | Error aceptable para considerar posición alcanzada |
| Settle time | 500 ms | Tiempo en tolerancia para confirmar llegada |
| PWM máximo | 700 | Límite de actuación durante pre-posicionamiento |
| Anti-windup | ±500 | Saturación de integral |

---

## Algoritmo PID

### Lazo externo — PID Posición (genera θ_ref)

#### Ecuación discreta

```
error_pos(k) = pos_setpoint - pos_actual_cm

integral_pos(k) = integral_pos(k-1) + error_pos(k) × dt

derivada_pos(k) = (error_pos(k) - error_pos(k-1)) / dt

derivada_filtrada_pos(k) = α × derivada_pos(k) + (1-α) × derivada_filtrada_pos(k-1)

output_pos(k) = Kp_pos × error_pos(k) + Ki_pos × integral_pos(k) + Kd_pos × derivada_filtrada_pos(k)
```

#### Zona muerta y saturación

```cpp
if (|error_pos| <= tolerancia_pos) {
    θ_ref = 0;                        // Zona muerta: no corregir posición
} else {
    θ_ref = -output_pos;              // Signo invertido (ver nota abajo)
    θ_ref = clamp(θ_ref, ±theta_ref_max);  // Saturar a ±2° por defecto
}
```

> **Inversión de signo:** El signo de θ_ref se invierte respecto a la salida PID. Si el carro está a la izquierda del setpoint (error positivo), θ_ref debe ser negativo para inclinar el péndulo a la izquierda, haciendo que el carro lo persiga hacia la derecha. Sin esta inversión, el carro se mueve en dirección contraria al setpoint.

### Lazo interno — PID Ángulo (genera PWM)

#### Ecuación discreta

```
setpoint_ang = θ_ref                                  // Del lazo externo
error_ang(k) = normalizar(θ_ref - ángulo_actual)     // [-180°, +180°]

integral_ang(k) = integral_ang(k-1) + error_ang(k) × dt

derivada_ang(k) = (error_ang(k) - error_ang(k-1)) / dt

derivada_filtrada_ang(k) = α × derivada_ang(k) + (1-α) × derivada_filtrada_ang(k-1)

output_ang(k) = Kp_ang × error_ang(k) + Ki_ang × integral_ang(k) + Kd_ang × derivada_filtrada_ang(k)
```

#### Aplicación al motor (con bias de zona muerta)

```cpp
if (|error_ang| <= tolerancia_ang) {
    Motor::freno();                    // Zona muerta angular
    outputSaturated = 0;
} else {
    pwm = |output_ang| + MOTOR_BIAS;   // Bias = 300 (zona muerta del motor)
    pwm = min(pwm, pwm_max);           // Saturar al máximo
    if (output_ang > 0) Motor::adelante(pwm);
    else                Motor::atras(pwm);
    outputSaturated = ±pwm;            // Con signo de dirección
}
```

### Diagrama de bloques

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                        Control PID Cascada                                     │
│                                                                                 │
│  ┌────────────────────────── Lazo Externo ──────────────────────────┐           │
│  │                                                                  │           │
│  │  pos_setpoint ──►(+)──► error ──► PID Pos ──► -output ──► clamp │           │
│  │  (45 cm)         │       pos    Kp,Ki,Kd     (invertido)  ±θmax  │           │
│  │                (-)                              │          │      │           │
│  │                  │                              │    ┌─────┘      │           │
│  │  HC-020K ────────┘                              │    │ zona       │           │
│  │  (pos cm)                                       │    │ muerta     │           │
│  │                                            |err|>tol?─┤           │           │
│  │                                              no→ θ=0  sí→ θ_ref  │           │
│  └──────────────────────────────────────────────────┼────────────────┘           │
│                                                     │                           │
│                                                     ▼ θ_ref (±2°)              │
│                                                                                 │
│  ┌────────────────────────── Lazo Interno ──────────────────────────┐           │
│  │                                                                  │           │
│  │  θ_ref ──►(+)──► error ──► PID Ang ──► output ──► +BIAS ──► PWM │           │
│  │           │       ang    Kp,Ki,Kd                  (300)    Motor │           │
│  │         (-)       norm.                                          │           │
│  │           │       ±180°                                          │           │
│  │  AS5600 ──┘                                                      │           │
│  │  (ángulo)                                                        │           │
│  └──────────────────────────────────────────────────────────────────┘           │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## Rango de trabajo y convención angular

### Convención angular

| Ángulo | Posición del péndulo |
|--------|---------------------|
| 0° / 360° | Vertical arriba (equilibrio inestable — **zona de control**) |
| 180° | Colgando abajo (equilibrio estable) |

El sensor AS5600 proporciona 0°–360° con wrapping. El error angular del lazo interno se normaliza a **[-180°, +180°]** para manejar correctamente la transición 360°/0°:

```cpp
// Ejemplo: ángulo = 358° (péndulo ligeramente inclinado), θ_ref = -1°
error = -1° - 358° = -359°
normalizado = -359° + 360° = +1°      // Necesita corregir +1°
```

### Rango de trabajo

El rango de trabajo define la banda angular desde 0° donde el control cascada se activa/desactiva:

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
│  │   (340°..360°)         (vertical)    (0°..20°)        │  │
│  │      CONTROL ACTIVO        ↑           CONTROL ACTIVO │  │
│  └───────────────────────────┬┬──────────────────────────┘  │
│                              ││                             │
│                   ┌──────────┘└──────────┐                  │
│                   │ FUERA DE RANGO       │                  │
│                   │ 20°...340°           │                  │
│                   │ → Freno → STOPPED    │                  │
│                   └─────────────────────┘                   │
│                                                             │
│  Con rango_trabajo = 20°:                                   │
│    Banda activa: [340°, 360°] ∪ [0°, 20°]                  │
│    Fuera de rango: (20°, 340°)                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Comportamiento:**
1. En estado `PIDC_READY`: si el ángulo entra en el rango de trabajo → auto-transiciona a `PIDC_RUNNING`
2. En estado `PIDC_RUNNING`: si el ángulo sale del rango de trabajo → freno → `PIDC_STOPPED`

### Zonas de seguridad

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
│                    (pos_setpoint)                                   │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

Protecciones durante PREPOSITIONING:
- Si un limit switch se activa **en la dirección de movimiento** → freno → STOPPED
- Si el carro está en zona de peligro **y se mueve hacia el límite** → freno → STOPPED

Protecciones durante RUNNING:
- **Bloqueo direccional**: solo frena si la salida del PID ángulo empuja **hacia** el límite activo
- Esto permite que el control corrija alejándose del límite sin provocar un stop innecesario

---

## Telemetría

### Modo dual: Normal (5 Hz) y Fast (~14-17 Hz)

La telemetría usa dos modos según el estado del control:

| Modo | Función | Frecuencia | Campos | Tamaño ~aprox |
|------|---------|------------|--------|---------------|
| **Normal** | `publishTelemetry()` | 5 Hz (200ms) | Todos (~40+ campos) | ~800 bytes |
| **Fast** | `publishTelemetryFast()` | 14-17 Hz (50ms target) | Campos esenciales | ~400 bytes |

La selección es automática: cuando `PidCascadaMode::isActive()` es `true` y el modo activo es `MODE_PID_CASC`, se usa Fast.

### Campos de telemetría PID Cascada (`pidc_*`)

#### Lazo externo (Posición)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `pidc_pos_error` | float | Error de posición: setpoint - pos_actual (cm) |
| `pidc_pos_integral` | float | Integral acumulada de posición |
| `pidc_pos_derivada` | float | Derivada filtrada de posición |
| `pidc_pos_output` | float | Salida PID cruda (antes de inversión y saturación) |
| `pidc_pos_output_sat` | float | θ_ref saturado (°, con signo invertido) |
| `pidc_pos_setpoint` | float | Setpoint de posición activo (cm) |
| `pidc_pos_kp` | float | Kp activo (posición) |
| `pidc_pos_ki` | float | Ki activo (posición) |
| `pidc_pos_kd` | float | Kd activo (posición) |
| `pidc_pos_zona_seg` | float | Zona de seguridad activa (cm) |

#### Lazo interno (Ángulo)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `pidc_ang_error` | float | Error angular normalizado: θ_ref - ángulo (°, ±180) |
| `pidc_ang_integral` | float | Integral acumulada angular |
| `pidc_ang_derivada` | float | Derivada filtrada angular |
| `pidc_ang_output` | float | Salida PID cruda ángulo |
| `pidc_ang_output_sat` | float | PWM aplicado con signo (±) |
| `pidc_ang_setpoint` | float | Setpoint angular = θ_ref (°, variable) |
| `pidc_ang_kp` | float | Kp activo (ángulo) |
| `pidc_ang_ki` | float | Ki activo (ángulo) |
| `pidc_ang_kd` | float | Kd activo (ángulo) |

#### Parámetros comunes

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `pidc_rango_trabajo` | float | Rango de trabajo activo (°) |
| `pidc_tolerancia_ang` | float | Tolerancia angular activa (°) |
| `pidc_tolerancia_pos` | float | Tolerancia posición activa (cm) |
| `pidc_pwm_min` | int | PWM mínimo configurado |
| `pidc_pwm_max` | int | PWM máximo configurado |
| `pidc_theta_ref_max` | float | Límite θ_ref activo (°) |

### Campos compartidos con telemetría base

| Campo | Tipo | Relevancia para PID cascada |
|-------|------|---------------------------|
| `angle_pend` | float | **Entrada lazo interno** — ángulo del péndulo (°) |
| `pos_ir_cm` | float | **Entrada lazo externo** — posición del carro (cm) |
| `pwm` | int | PWM aplicado al motor |
| `direction` | int | Dirección motor (1=ADL, -1=ATR, 0=DET) |
| `ol_state` | string | Estado del modo (`PIDC_RUNNING`, etc.) |
| `ls_start`, `ls_end` | bool | Estado de limit switches |

### Ejemplo de telemetría durante PIDC_RUNNING

```json
{
  "ts": 1743930000,
  "modo": "PID_CASC",
  "ol_state": "PIDC_RUNNING",
  "angle_pend": 359.2,
  "pos_ir_cm": 43.5,
  "pwm": 340,
  "direction": 1,
  "pidc_pos_error": 1.5,
  "pidc_pos_integral": 0.8,
  "pidc_pos_derivada": -0.2,
  "pidc_pos_output": 0.95,
  "pidc_pos_output_sat": -0.95,
  "pidc_pos_setpoint": 45.0,
  "pidc_pos_kp": 0.5,
  "pidc_pos_ki": 0.01,
  "pidc_pos_kd": 0.1,
  "pidc_pos_zona_seg": 5.0,
  "pidc_ang_error": -1.75,
  "pidc_ang_integral": -2.1,
  "pidc_ang_derivada": 0.3,
  "pidc_ang_output": -40.0,
  "pidc_ang_output_sat": -340.0,
  "pidc_ang_setpoint": -0.95,
  "pidc_ang_kp": 40.0,
  "pidc_ang_ki": 1.0,
  "pidc_ang_kd": 0.5,
  "pidc_rango_trabajo": 20.0,
  "pidc_tolerancia_ang": 0.1,
  "pidc_tolerancia_pos": 0.5,
  "pidc_pwm_min": 250,
  "pidc_pwm_max": 900,
  "pidc_theta_ref_max": 2.0,
  "ls_start": false,
  "ls_end": false
}
```

### Telemetría por estado

| Estado | Motor activo | Telemetría | Frecuencia |
|--------|:------------:|-----------|:----------:|
| `PIDC_IDLE` | No | Normal | 5 Hz |
| `PIDC_HOMING_START` | Sí | Fast | 20 Hz |
| `PIDC_HOMING_END` | Sí | Fast | 20 Hz |
| `PIDC_CALIBRATING` | No | Normal | 5 Hz |
| `PIDC_WAIT_READY` | No | Normal | 5 Hz |
| `PIDC_PREPOSITIONING` | Sí | Fast | 20 Hz |
| `PIDC_READY` | No | Normal | 5 Hz |
| `PIDC_RUNNING` | Sí | Fast | 20 Hz |
| `PIDC_STOPPED` | No | Normal | 5 Hz |

---

## Comunicación MQTT

### Flujo de datos

```
┌────────────┐     ┌───────────┐     ┌────────────┐     ┌────────────┐
│   ESP32    │─MQTT─►           │─MQTT─►            │─ WS ─►            │
│  Firmware  │     │ Mosquitto │     │  Backend   │     │ Frontend   │
│  (SPISC)   │◄────│  :1883    │◄────│  :8000     │◄────│  :5173     │
└────────────┘     └───────────┘     └────────────┘     └────────────┘
     WiFi              TCP             HTTP/WS          Navegador
```

- **ESP32 → Backend:** Telemetría + estado por MQTT. Backend recibe con suscripción wildcard `pendulum/#` y reenvía al frontend vía WebSocket.
- **Frontend → ESP32:** Comandos viajan por WebSocket → Backend → publish MQTT al tópico `pendulum/cmd/pidcasc`. El firmware los recibe en Core 1 y los encola en cola FreeRTOS (máx. 10) para Core 0.

### Tópicos relevantes

| Tópico | Dirección | Contenido |
|--------|-----------|-----------|
| `pendulum/telemetry` | ESP32 → Backend | Telemetría completa (normal 5Hz / fast 20Hz) |
| `pendulum/status` | ESP32 → Backend | Estado del sistema (2 Hz) |
| `pendulum/cmd/pidcasc` | Backend → ESP32 | Comandos del modo cascada |
| `pendulum/cmd/stop` | Backend → ESP32 | Parada de emergencia global |
| `pendulum/cmd/mode` | Backend → ESP32 | Cambio de modo de operación |

### Parámetros técnicos clave

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| **Buffer MQTT (PubSubClient)** | 2048 bytes | Tamaño máximo de mensaje MQTT. Aumentado de 1024 para acomodar los 23 campos pidc_*. |
| **MAX_SUBS** | 16 | Máximo de suscripciones MQTT simultáneas. |
| **QoS** | 0 (fire-and-forget) | Sin confirmación de entrega. |
| **Keep-alive MQTT** | 5 s | Ping periódico al broker. |
| **TCP NoDelay** | Activado | Elimina latencia del algoritmo de Nagle. |
| **Safety timeout MQTT** | 10 s | Freno automático si no llegan mensajes en 10s. |
| **Cola de comandos** | Máx. 10 | Cola FreeRTOS para comandos MQTT (Core 1 → Core 0). |

---

## Guía de uso

### Flujo típico de operación

```
1. Activar modo PID Cascada
   └─► switchMode(MODE_PID_CASC)
       └─► begin() → Auto-homing si no está en LS_START

2. Configurar parámetros PID (opcional)
   └─► Enviar config con Kp/Ki/Kd de ambos lazos, θ_ref_max, tolerancias
       └─► Estado: PIDC_IDLE (configuración aplicada)

3. Ejecutar secuencia
   └─► Enviar comando run
       └─► Homing a LS_START → Calibración (200ms) → Wait (2s)
           └─► Pre-posicionamiento a 45 cm (PID pos fijo)
               └─► Estado: PIDC_READY

4. Levantar péndulo a vertical
   └─► Ángulo entra en rango de trabajo (ej: 350°-360° ó 0°-10°)
       └─► Auto-transición a PIDC_RUNNING
           └─► Control cascada activo a 1000 Hz

5. Control activo
   └─► Lazo externo genera θ_ref según error de posición
       └─► Lazo interno estabiliza péndulo en θ_ref
           └─► Si péndulo cae (sale del rango):
               └─► Freno → STOPPED
                   └─► Reiniciar con cmd:run → vuelve al paso 3

6. Detener
   └─► Enviar comando stop
       └─► Estado: PIDC_STOPPED
```

### Consideraciones importantes

1. **Siempre ejecutar `run` primero**: Inicia la secuencia completa (homing → calibración → wait → pre-posicionamiento → ready). Sin esto, el carro puede no estar centrado.

2. **θ_ref es variable**, a diferencia del PID Ángulo (fijo 0°). El lazo externo genera una pequeña inclinación de referencia para que el carro se mueva hacia el setpoint de posición.

3. **La transición a RUNNING es automática**: No se requiere presionar EJECUTAR. Basta con levantar el péndulo a la zona vertical.

4. **La inversión de signo de θ_ref es crítica**: Sin ella, el carro se mueve en dirección contraria al setpoint.

5. **Tolerancia de posición**: Cuando el carro está cerca del setpoint (dentro de `tolerancia_pos`), θ_ref = 0 y el sistema solo estabiliza el péndulo verticalmente sin intentar corregir posición.

6. **Ajuste de ganancias — lazo externo:**
   - Kp_pos bajo → respuesta lenta de posición
   - Kp_pos alto → oscilaciones de posición (θ_ref cambia bruscamente)
   - theta_ref_max bajo → corrección de posición suave pero lenta
   - theta_ref_max alto → corrección agresiva, riesgo de desestabilización

7. **Ajuste de ganancias — lazo interno:**
   - Se comporta como el PID Ángulo estándar pero con setpoint variable
   - Los mismos criterios de tuning aplican (Kp=oscilación, Kd=amortiguamiento, Ki=offset)

---

## Ejemplos de comandos

### Secuencia básica completa

```bash
# 1. Activar modo PID Cascada
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/mode" \
  -m '{"mode":"PID_CASC"}'

# 2. Configurar parámetros
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","kp_pos":0.5,"ki_pos":0.01,"kd_pos":0.1,"pos_setpoint":45,"theta_ref_max":2,"tolerancia_pos":0.5,"kp_ang":40,"ki_ang":1,"kd_ang":0.5,"rango_trabajo":20,"tolerancia_ang":0.1}'

# 3. Iniciar secuencia completa
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidcasc" \
  -m '{"action":"run"}'

# 4. [Levantar el péndulo a la vertical → auto-transición a RUNNING]

# 5. Detener si es necesario
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidcasc" \
  -m '{"action":"stop"}'
```

### Comandos rápidos

```bash
# Ir a home
mosquitto_pub -t "pendulum/cmd/pidcasc" -m '{"action":"home"}'

# Stop de emergencia (global)
mosquitto_pub -t "pendulum/cmd/stop" -m '{}'

# Solo cambiar ganancias del lazo externo
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","kp_pos":1.0,"theta_ref_max":3}'

# Solo cambiar ganancias del lazo interno
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","kp_ang":50,"kd_ang":1.0}'

# Reset a IDLE
mosquitto_pub -t "pendulum/cmd/pidcasc" -m '{"action":"reset"}'
```

### Monitoreo de telemetría

```bash
# Suscribirse a telemetría
mosquitto_sub -h 192.168.4.2 -t "pendulum/telemetry" -v

# Filtrar campos cascada (con jq)
mosquitto_sub -h 192.168.4.2 -t "pendulum/telemetry" | \
  jq '{state: .ol_state, angle: .angle_pend, pos: .pos_ir_cm, theta_ref: .pidc_pos_output_sat, ang_err: .pidc_ang_error, pwm: .pwm}'
```

---

## Troubleshooting

### Problema: El carro se mueve en dirección contraria al setpoint

**Causa:** Signo de θ_ref incorrecto — la inversión `θ_ref = -output_pos` no está aplicada.

**Verificación:** Observar `pidc_pos_output_sat` en telemetría. Si el carro está a la izquierda del setpoint (pos < 45), θ_ref debe ser **negativo** (inclinación izquierda para que el carro vaya a la derecha).

### Problema: El péndulo no se estabiliza (oscila y cae)

**Posibles causas:**
1. Kp_ang muy alto → oscilación violenta
2. Kd_ang muy bajo → sin amortiguamiento
3. theta_ref_max muy alto → el lazo externo genera referencias angulares demasiado grandes
4. Rango de trabajo muy estrecho → PID se desactiva con pequeña desviación

**Soluciones:**
```bash
# Reducir Kp_ang, aumentar Kd_ang
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","kp_ang":30,"kd_ang":1.0}'

# Reducir theta_ref_max para correcciones más suaves
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","theta_ref_max":1.0}'
```

### Problema: El carro oscila lentamente alrededor del setpoint

**Causa:** Kp_pos alto o tolerancia_pos baja — el lazo externo genera θ_ref continuamente, alternando la dirección.

**Soluciones:**
```bash
# Aumentar tolerancia de posición (zona muerta más amplia)
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","tolerancia_pos":2.0}'

# Reducir ganancias del lazo externo
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","kp_pos":0.3}'
```

### Problema: El PID no se activa (queda en READY)

**Posibles causas:**
1. El péndulo no alcanza el rango de trabajo
2. Rango de trabajo configurado muy estrecho

**Soluciones:**
```bash
# Ampliar rango de trabajo
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","rango_trabajo":25}'

# Verificar ángulo en telemetría
mosquitto_sub -t "pendulum/telemetry" | jq '.angle_pend'
```

### Problema: El control se detiene inmediatamente al activarse (RUNNING → STOPPED)

**Posibles causas:**
1. El carro no fue pre-posicionado (problema resuelto en v1.1)
2. Limit switch activo + motor empujando hacia él
3. Péndulo fuera de rango justo después de entrar

**Verificación:** Revisar el log serial — indicará la causa (límite, rango, o zona de seguridad).

### Problema: El carro vibra sin moverse significativamente

**Posibles causas:**
1. Tolerancia angular muy baja
2. Kp_ang alto con Kd_ang bajo

**Soluciones:**
```bash
mosquitto_pub -t "pendulum/cmd/pidcasc" \
  -m '{"action":"config","tolerancia_ang":0.5,"kd_ang":1.0}'
```

---

## Protecciones de seguridad

El modo PID Cascada incluye 7 capas de protección:

| # | Protección | Activación | Acción |
|---|-----------|------------|--------|
| 1 | **Limit switches (direccional)** | LS activo + motor empujando HACIA ese límite | Freno → STOPPED |
| 2 | **Zona de peligro (PREPOSITIONING)** | pos en zona peligro + movimiento hacia límite | Freno → STOPPED |
| 3 | **Timeout homing** | 30s sin alcanzar limit switch | Freno → STOPPED |
| 4 | **Rango de trabajo** | Ángulo fuera de banda configurable | Freno → STOPPED |
| 5 | **Parada de emergencia** | `pendulum/cmd/stop` (global) | Freno inmediato → STOPPED |
| 6 | **Anti-windup** | Integral PID (ambos lazos) | Clamp a ±500 |
| 7 | **Saturación θ_ref** | Salida del lazo externo | Clamp a ±theta_ref_max |

---

## Archivos del sistema

### Firmware (SPISC/)

| Archivo | Descripción |
|---------|-------------|
| `include/telemetry_data.h` | Enum `MODE_PID_CASC` + 23 campos `pidc_*` en `TelemetryPacket` |
| `include/hardware_pins.h` | `MOTOR_BIAS` (300), `TRACK_LENGTH_CM` (90), `HOMING_PWM` (300) |
| `include/wifi_mqtt_config.h` | `TOPIC_CMD_PID_CASC "pendulum/cmd/pidcasc"` |
| `src/modes/pid_cascada_mode.h` | Header — interfaz del namespace (26 funciones públicas) |
| `src/modes/pid_cascada_mode.cpp` | Implementación completa (~850 líneas): máquina de estados, PID cascada, pre-posicionamiento |
| `src/main.cpp` | 7 puntos de integración (include, switchMode, stateStr, routing ×3, buildTelemetry, taskLoop) |
| `src/mqtt_comm.cpp` | Suscripción a `pendulum/cmd/pidcasc` + serialización JSON pidc_* (normal y fast) |
| `lib/MqttBridge/` | Buffer 2048 bytes, MAX_SUBS=16 |

### Backend (hmi/backend/)

| Archivo | Descripción |
|---------|-------------|
| `config.py` | `TOPICO_CMD_PID_CASC = "pendulum/cmd/pidcasc"` |
| `websocket_manager.py` | Ruta `cmd_pidcasc` → publica en MQTT |

### Frontend (hmi/frontend/src/)

| Archivo | Descripción |
|---------|-------------|
| `stores/pendulumStore.js` | Estado `pidCascState` (17 campos), acciones `configurarPidCasc`, normalización de campos `pidc_*` |
| `components/tabs/TabControlPidCascada.vue` | Panel de control: parámetros de ambos lazos, botones, indicadores, badges de estado |
| `components/ResultadosPidCascada.vue` | Tabla de resultados + exportación CSV |
| `components/GraficosTelemetria.vue` | Gráficos con series `pidc_*` |
| `components/PaginaInicio.vue` | Tarjeta de navegación del experimento |
| `App.vue` | Ruta `pid_cascada` → `TabControlPidCascada` |

---

## Estado actual del sistema

### Qué funciona (validado en hardware — 06/04/2026)

- Máquina de estados completa con 9 estados y transiciones correctas
- Pre-posicionamiento automático del carro a 45 cm antes de habilitar control
- Auto-transición READY → RUNNING cuando péndulo entra en rango de trabajo
- Bloqueo direccional de límites (solo frena si motor empuja hacia el límite)
- Control cascada a 1000 Hz con ambos lazos ejecutándose en el mismo ciclo
- Inversión de signo de θ_ref para corrección de posición en dirección correcta
- Tolerancia de posición configurable (zona muerta del lazo externo)
- Homing automático a LS_START
- Telemetría: 23 campos `pidc_*` (10 pos + 10 ang + 6 comunes - 3 compartidos)
- Telemetría dinámica (fast durante PID activo, normal en reposo)
- HMI completo: parámetros de ambos lazos, indicadores, gráficos, resultados con CSV
- Configuración dinámica de todos los parámetros vía MQTT/HMI
- Compilación exitosa: RAM ~25.2%, Flash ~64.8%

### Bugs corregidos (v1.0 → v1.1)

| Bug | Causa raíz | Corrección |
|-----|------------|------------|
| Comunicación no funcionaba | Buffer PubSubClient 1024 bytes insuficiente para pidc_* | Aumentado a 2048 bytes |
| No cambiaba a modo cascada | MAX_SUBS=10 pero 12 suscripciones | Aumentado a 16 |
| READY→RUNNING no funcionaba | Prioridad invertida: cmdRun consumido por homing antes de check de estado | Reordenado: READY check antes de home/end/run |
| RUNNING→STOPPED inmediato | Cart en pos=0 (LS_START) tras homing, sin pre-posicionamiento | Agregado estado PIDC_PREPOSITIONING |
| READY requería botón EJECUTAR | Solo verificaba rango al presionar botón (one-shot) | Auto-transición continua como PID Ángulo |
| Límites demasiado agresivos | `startActive \|\| endActive → STOPPED` sin dirección | Bloqueo direccional: solo si motor empuja hacia límite |
| Carro se movía al revés | θ_ref mismo signo que error_pos | Inversión: `θ_ref = -output_pos` |

### Limitaciones conocidas

- **Sin reposicionamiento automático**: Si el péndulo cae, va a STOPPED (requiere cmd:run manual). El PID Ángulo tiene REPOSITIONING automático.
- **Ganancias por calibrar**: Los valores por defecto de ambos lazos son un punto de partida y requieren ajuste experimental.
- **Sin Swing-up**: El péndulo debe ser levantado manualmente a la zona vertical.

---

## Mejoras futuras propuestas

### 1. Ajuste fino de ganancias cascada (Prioridad: Alta)

Realizar sesiones de tuning sistemático:
1. Primero ajustar lazo interno (Kp_ang, Kd_ang) con θ_ref fijo = 0 (equivalente a PID Ángulo)
2. Una vez estabilizado, activar lazo externo con Kp_pos bajo y subir gradualmente
3. Ajustar theta_ref_max y tolerancia_pos según comportamiento observado

### 2. Estado PIDC_REPOSITIONING (Prioridad: Media)

Agregar un estado de reposicionamiento automático (como PID Ángulo) para que cuando el péndulo caiga, el carro regrese al centro en vez de quedarse en STOPPED. Flujo: RUNNING → (péndulo cae) → REPOSITIONING → READY → (péndulo entra) → RUNNING.

### 3. Swing-up automático (Prioridad: Media)

Implementar algoritmo de energía para llevar el péndulo de 180° a 0° automáticamente, transicionando suavemente al control cascada cuando alcance el rango de trabajo.

### 4. Fusión de sensores (Prioridad: Baja)

Combinar AS5600 (posición angular) con MPU6050 (velocidad angular via giroscopio) usando filtro complementario para mejorar la estimación del estado del péndulo.

---

## Referencias

- [Control PID de Ángulo](control_pid_angulo.md) — Modo PID de ángulo standalone (referencia de implementación del lazo interno)
- [Control PID de Posición](control_pid_posicion.md) — Modo PID de posición del carro (referencia de implementación del lazo externo)
- [Calibración Sensor Magnético](calibracion_posicion_mag.md) — Calibración del AS5600
- [Arquitectura Dual-Core](config_dual_core.md) — Ejecución en Core 0/1
- [Plan de Mejoras](plan_mejoras.md) — Mejoras globales del sistema
