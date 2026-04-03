# Control PID de Posición del Carro

**Proyecto:** [SPISC — Sistema de Péndulo Invertido con Supervisión y Control](../README.md)  
**Fecha:** 01/04/2026  
**Última actualización:** 03/04/2026 (v3.0)  
**Autor:** Ing. Jeisson Martínez Flórez

---

## Tabla de contenidos

- [Descripción general](#descripción-general)
- [Arquitectura del modo](#arquitectura-del-modo)
- [Máquina de estados](#máquina-de-estados)
- [Comandos MQTT](#comandos-mqtt)
- [Parámetros configurables](#parámetros-configurables)
- [Algoritmo PID](#algoritmo-pid)
- [Telemetría](#telemetría)
- [Guía de uso](#guía-de-uso)
- [Troubleshooting](#troubleshooting)
- [Problemas conocidos y soluciones](#problemas-conocidos-y-soluciones)
- [Optimizaciones de telemetría (v3.0)](#optimizaciones-de-telemetría-v30)
- [Estado actual del sistema](#estado-actual-del-sistema)
- [Mejoras futuras propuestas](#mejoras-futuras-propuestas)
- [Referencias](#referencias)

---

## Descripción general

El modo **Control PID de Posición** (`MODE_PID_POS`) implementa un controlador PID discreto para posicionar el carro del péndulo invertido en cualquier punto del riel. Este modo:

- Reutiliza la lógica de homing del modo Lazo Abierto
- Incluye zonas de seguridad configurables para proteger los límites del riel
- Expone telemetría extendida con error, integral, derivada y salida del controlador
- Permite configuración dinámica de ganancias Kp, Ki, Kd via MQTT

### Principio de funcionamiento

```
┌─────────────────────────────────────────────────────────────┐
│  Flujo de Control PID                                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Setpoint (cm) ──┬──► (+) ──► Controlador  ──► Motor      │
│                   │     │      PID               PWM       │
│                   │     │                                   │
│                   │   (-)                                   │
│                   │     │                                   │
│   Sensor ─────────┴─────┘                                   │
│   (HC-020K IR — activo; AS5600 magnético — en revisión)     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Características principales

| Característica | Valor por defecto | Rango configurable |
|---------------|-------------------|---------------------|
| **Frecuencia de control** | 1000 Hz (Core 0) | Fijo |
| **Sensor de posición** | HC-020K IR (activo) | `mag` / `ir` |
| **Rango de posición** | 0 - 90 cm | Fijo |
| **PWM mínimo** | 200 | [100, 500] |
| **PWM máximo** | 700 | [300, 1023] |
| **Anti-windup** | Saturación de integral ±500 | Fijo |
| **Filtro derivativo** | Pasa-bajos α = 0.2 | Fijo |
| **Telemetría (PID activo)** | ~14-17 Hz (fast) | Fijo 50ms target |
| **Telemetría (reposo)** | 5 Hz (normal) | Fijo 200ms |

> **Nota v3**: El sensor IR es el activo por defecto. El AS5600 magnético está en revisión por problema de hardware (no reporta movimiento). El sensor y límites PWM son configurables vía MQTT.

---

## Arquitectura del modo

### Archivos involucrados

```
SPISC/
├── src/modes/
│   ├── pid_posicion_mode.h     ← Header con interfaz pública
│   └── pid_posicion_mode.cpp   ← Implementación del modo
├── include/
│   ├── telemetry_data.h        ← Campos PID en TelemetryPacket + flag telemetry_fast
│   ├── hardware_pins.h         ← TELEMETRY_INTERVAL_MS, TELEMETRY_FAST_INTERVAL_MS
│   └── wifi_mqtt_config.h      ← TOPIC_CMD_PID_POS
├── src/
│   ├── main.cpp                ← Orquestador dual-core, telemetría dinámica
│   ├── mqtt_comm.cpp           ← publishTelemetry() + publishTelemetryFast()
│   └── encoder_ir.cpp          ← Posición con dirección motor/magnético
└── lib/
    └── MqttBridge/             ← Conexión WiFi+MQTT (setNoDelay)
```

### Namespace

```cpp
namespace PidPosicionMode {
    void begin();           // Inicializa modo (auto-homing)
    void loop();            // Loop de control (500-1000 Hz)
    void onCommand(...);    // Procesa comandos MQTT
    const char* getStateStr();
    bool isActive();
    
    // Getters para telemetría
    float getError();
    float getIntegral();
    float getDerivada();
    float getOutput();
    float getSetpoint();
    float getKp(), getKi(), getKd();
    float getZonaSeguridad();
    float getTolerancia();
}
```

---

## Máquina de estados

```
┌──────────────────────────────────────────────────────────────────────┐
│                    Diagrama de Estados PID_POS                       │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌──────────┐  auto-home   ┌─────────────────┐                     │
│   │ PID_IDLE │─────────────►│ PID_HOMING_START │                    │
│   └──────────┘              └────────┬────────┘                     │
│        │                             │                               │
│        │ ya en home                  │ LS_START activo               │
│        │                             ▼                               │
│        └──────────────────►┌───────────┐                            │
│                            │ PID_READY │◄──────────────────┐        │
│   ┌─────────────────┐      └─────┬─────┘                   │        │
│   │ PID_HOMING_END  │◄───────────┼───────── cmd:end        │        │
│   └────────┬────────┘            │                         │        │
│            │                     │ cmd:run                 │        │
│            │ LS_END activo       ▼                         │        │
│            └───────────────►┌─────────────┐                │        │
│                             │ PID_RUNNING │                │        │
│                             └──────┬──────┘                │        │
│                                    │                       │        │
│          ┌─────────────────────────┼─────────────────┐     │        │
│          │                         │                 │     │        │
│          ▼                         ▼                 ▼     │        │
│   ┌─────────────┐         ┌──────────────┐   ┌───────────┐│        │
│   │ PID_STOPPED │         │ PID_AT_TARGET│   │limit/zona ││        │
│   │  (cmd:stop) │         │(setpoint OK) │   │seguridad  ││        │
│   └──────┬──────┘         └──────┬───────┘   └─────┬─────┘│        │
│          │                       │                 │      │        │
│          └───────────────────────┴─────────────────┴──────┘        │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

### Descripción de estados

| Estado | Descripción |
|--------|-------------|
| `PID_IDLE` | Estado inicial. Auto-inicia homing si no está en home. |
| `PID_HOMING_START` | Moviendo hacia LS_START (home). Timeout: 30s. |
| `PID_HOMING_END` | Moviendo hacia LS_END. Timeout: 30s. |
| `PID_READY` | Listo para configurar y ejecutar control PID. |
| `PID_RUNNING` | Control PID activo. Monitorea límites y zona segura. |
| `PID_AT_TARGET` | Setpoint alcanzado (error < tolerancia por 500ms). |
| `PID_STOPPED` | Detenido por comando, límite o zona de seguridad. |

---

## Comandos MQTT

**Tópico:** `pendulum/cmd/pidpos`

### Tabla de comandos

| Acción | Payload JSON | Desde estados | Descripción |
|--------|--------------|---------------|-------------|
| `home` | `{"action":"home"}` | IDLE, READY, STOPPED, AT_TARGET | Mover a LS_START |
| `end` | `{"action":"end"}` | IDLE, READY, STOPPED, AT_TARGET | Mover a LS_END |
| `config` | Ver debajo | IDLE, READY, STOPPED, AT_TARGET | Configurar parámetros PID |
| `run` | `{"action":"run"}` | READY, STOPPED, AT_TARGET | Iniciar control PID |
| `stop` | `{"action":"stop"}` | Cualquiera | Detener inmediatamente |
| `reset` | `{"action":"reset"}` | Cualquiera | Volver a PID_IDLE |

### Payload de configuración

```json
{
  "action": "config",
  "kp": 8.0,
  "ki": 0.1,
  "kd": 0.5,
  "setpoint": 45.0,
  "zona_seguridad": 5.0,
  "tolerancia": 1.0,
  "sensor": "mag",
  "pwm_min": 200,
  "pwm_max": 700
}
```

> **Nota:** Todos los parámetros son opcionales. Solo se actualizan los incluidos.

#### Selección de sensor

| Valor | Sensor | Descripción |
|-------|--------|-------------|
| `"mag"` | AS5600 magnético | Mayor precisión, requiere calibración |
| `"ir"` | HC-020K infrarrojo | Más robusto, menor resolución |

#### Límites de PWM

Permite ajustar el rango del actuador:
- `pwm_min`: PWM mínimo para vencer inercia (default: 200, rango: [100, 500])
- `pwm_max`: PWM máximo (default: 700, rango: [300, 1023])

---

## Parámetros configurables

| Parámetro | Default | Rango | Unidad | Descripción |
|-----------|---------|-------|--------|-------------|
| `kp` | 8.0 | [0.1, 50.0] | - | Ganancia proporcional |
| `ki` | 0.1 | [0.0, 10.0] | - | Ganancia integral |
| `kd` | 0.5 | [0.0, 5.0] | - | Ganancia derivativa |
| `setpoint` | 45.0 | [zona_seg, 90-zona_seg] | cm | Posición objetivo |
| `zona_seguridad` | 5.0 | [2.0, 20.0] | cm | Distancia segura desde límites |
| `tolerancia` | 1.0 | [0.5, 5.0] | cm | Error aceptable para "llegada" |
| `sensor` | "mag" | mag, ir | - | Sensor de posición |
| `pwm_min` | 200 | [100, 500] | - | PWM mínimo (vencer inercia) |
| `pwm_max` | 700 | [300, 1023] | - | PWM máximo |

### Validación de setpoint

El setpoint debe estar dentro de la **zona segura**:

```
Zona segura = [zona_seguridad, TRACK_LENGTH - zona_seguridad]

Con zona_seguridad = 5 cm y TRACK_LENGTH = 90 cm:
Zona segura = [5, 85] cm

Setpoints válidos: 5.0 cm ... 85.0 cm
Setpoints inválidos: 0-4.9 cm y 85.1-90 cm
```

---

## Algoritmo PID

### Ecuación discreta

```
error(k) = setpoint - posición_actual

integral(k) = integral(k-1) + error(k) × dt

derivada(k) = (error(k) - error(k-1)) / dt

derivada_filtrada(k) = α × derivada(k) + (1-α) × derivada_filtrada(k-1)

output(k) = Kp × error(k) + Ki × integral(k) + Kd × derivada_filtrada(k)
```

Donde:
- `dt` = período de muestreo (~1-2 ms)
- `α` = 0.2 (coeficiente de filtro pasa-bajos)

### Anti-windup

La integral se satura para evitar acumulación excesiva:

```cpp
if (integral > INTEGRAL_MAX) integral = INTEGRAL_MAX;    // +500
if (integral < -INTEGRAL_MAX) integral = -INTEGRAL_MAX;  // -500
```

### Aplicación al motor

```cpp
if (|error| <= tolerancia) {
    Motor::freno();
} else {
    pwm = clamp(|output|, MIN_PWM=200, MAX_PWM=1023);
    if (output > 0) Motor::adelante(pwm);
    else            Motor::atras(pwm);
}
```

### Diagrama de bloques

```
                    ┌─────────────────────────────────────────────────┐
                    │              Controlador PID                    │
                    │                                                 │
 Setpoint ──┬──►(+)─┼──► error ──┬──► Kp ──────────────────┐         │
            │   │   │            │                         │         │
            │  (-) ◄┼────────────┼─────────────────────────┼─────┐   │
            │       │            │                         │     │   │
            │       │            ├──► ∫ ──► Ki ────────────┤     │   │
            │       │            │         (anti-windup)   │     │   │
            │       │            │                         ▼     │   │
            │       │            └──► d/dt ──► Kd ──► Σ ──►PWM  │   │
            │       │                (filtrado)            │     │   │
            │       │                                      │     │   │
            │       └──────────────────────────────────────┼─────┘   │
            │                                              │         │
            │                                              ▼         │
            │                                           Motor        │
            │                                              │         │
            │                                              ▼         │
            └─────◄───── HC-020K IR ◄── Posición ◄───── Carro        │
                                                                     │
                    └─────────────────────────────────────────────────┘
```

---

## Zonas de seguridad

### Concepto

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
│     │←─────►│                                  │←──────────►│      │
│     zona_seg                                     zona_seg         │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### Comportamiento de protección

1. **Setpoint inválido**: Si el setpoint está fuera de la zona segura, se rechaza con mensaje de error.

2. **Límite físico alcanzado**: Si un limit switch se activa durante el control, se frena inmediatamente y pasa a `PID_STOPPED`.

3. **Zona de peligro**: Si el carro entra en zona de peligro Y se aleja del setpoint hacia el límite, se frena.

---

## Telemetría extendida

### Modo dual: Normal (5 Hz) y Fast (20 Hz target)

Desde v3.0, la telemetría usa dos modos según el estado del control:

| Modo | Función | Frecuencia | Campos | Tamaño ~aprox |
|------|---------|------------|--------|---------------|
| **Normal** | `publishTelemetry()` | 5 Hz (200ms) | Todos (~30 campos) | ~500 bytes |
| **Fast** | `publishTelemetryFast()` | 14-17 Hz (50ms target) | 19 campos esenciales | ~170 bytes |

La selección es automática: cuando `PidPosicionMode::isActive()` es `true` y el modo activo es `MODE_PID_POS`, se usa Fast. En cualquier otro caso, Normal.

```cpp
// En main.cpp — buildTelemetry()
pkt.telemetry_fast = (activeMode == MODE_PID_POS) && PidPosicionMode::isActive();

// En taskCommunication()
if (pkt.telemetry_fast)
    MqttComm::publishTelemetryFast(pkt);  // Compact JSON
else
    MqttComm::publishTelemetry(pkt);       // Full JSON
```

### Campos en modo Fast (durante PID_RUNNING)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `ts` | ulong | Timestamp sincronizado |
| `modo` | string | Modo firmware (`PID_POS`) |
| `ol_state` | string | Estado del modo (`PID_RUNNING`, etc.) |
| `pulsos_ir` | ulong | Conteo raw de pulsos encoder IR |
| `pos_ir_cm` | float | Posición encoder IR en cm |
| `pos_mag_cm` | float | Posición magnética en cm |
| `angle_pend` | float | Ángulo del péndulo (grados) |
| `pwm` | int | PWM aplicado al motor |
| `direction` | int | Dirección motor (1=ADL, -1=ATR, 0=DET) |
| `pid_error` | float | Error actual (setpoint - pos_actual) en cm |
| `pid_output` | float | Salida cruda del controlador |
| `pid_output_sat` | float | Salida saturada |
| `pid_setpoint` | float | Setpoint configurado en cm |
| `pid_integral` | float | Término integral acumulado |
| `pid_derivada` | float | Término derivativo filtrado |
| `rpm_ir` | float | RPM encoder IR |
| `rpm_mag` | float | RPM magnético |
| `ls_start` | bool | Limit switch inicio |
| `ls_end` | bool | Limit switch fin |

### Campos adicionales solo en modo Normal

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `vueltas_ir`, `rps_ir`, `rps_mag` | float | Vueltas y RPS |
| `vel_ang_pend` | float | Velocidad angular del péndulo |
| `mpu_ax/ay/az`, `mpu_gx/gy/gz` | float | Acelerómetro + giroscopio MPU6050 |
| `mpu_pitch`, `mpu_roll` | float | Ángulos MPU |
| `pid_kp`, `pid_ki`, `pid_kd` | float | Ganancias PID (no cambian durante ejecución) |
| `pid_zona_seg`, `pid_tolerancia` | float | Config PID (no cambia) |
| `pid_sensor_type`, `pid_pwm_min`, `pid_pwm_max` | int | Config PID (no cambia) |

> **Nota v3**: Los campos omitidos en modo Fast son **datos estáticos** (ganancias, configuración) que no cambian durante la ejecución del PID, y **datos del MPU6050** que no se usan en control de posición. El frontend usa `?? 0` (nullish coalescing) para manejar campos ausentes sin errores.

### Optimizaciones asociadas

1. **OLED skip (P1)**: Durante `telemetry_fast=true`, se omite la actualización del display OLED para liberar tiempo de CPU en Core 1 para comunicaciones.

2. **TCP Nagle disabled (P0)**: `WiFiClient.setNoDelay(true)` en `MqttBridge::begin()` desactiva el algoritmo de Nagle, reduciendo latencia de envío.

### Ejemplo de telemetría Fast

```json
{
  "ts": 1743641470,
  "modo": "PID_POS",
  "ol_state": "PID_RUNNING",
  "pulsos_ir": 2377,
  "pos_ir_cm": 23.71,
  "pos_mag_cm": 0.00,
  "angle_pend": 175.30,
  "pwm": 489,
  "direction": 1,
  "pid_error": 21.59,
  "pid_output": 489.0,
  "pid_output_sat": 489.0,
  "pid_setpoint": 45.0,
  "pid_integral": 12.3,
  "pid_derivada": -5.2,
  "rpm_ir": 2250.0,
  "rpm_mag": 0.0,
  "ls_start": false,
  "ls_end": false
}
```

---

## Guía de uso

### Flujo típico de operación

```
1. Encender sistema
   └─► Modo se activa automáticamente
       └─► Auto-homing a LS_START
           └─► Estado: PID_READY

2. Configurar parámetros PID
   └─► Enviar comando config con Kp, Ki, Kd, setpoint
       └─► Estado: PID_READY (configurado)

3. Ejecutar control
   └─► Enviar comando run
       └─► Estado: PID_RUNNING
           └─► Carro se mueve hacia setpoint

4. Llegada a setpoint
   └─► Error < tolerancia por 500ms
       └─► Estado: PID_AT_TARGET

5. Nuevo ciclo (opcional)
   └─► Reconfigurar setpoint
       └─► Enviar run
           └─► Repetir desde paso 3
```

### Consideraciones importantes

1. **Siempre hacer homing primero** si el sistema se reinició o perdió referencia.

2. **Configurar antes de run**: El setpoint por defecto es 45 cm (centro del riel).

3. **Monitorear telemetría**: Observar `pid_error` y `pid_output` para ajustar ganancias.

4. **Ajuste de ganancias**:
   - Si oscila: ↓ Kp, ↑ Kd
   - Si es lento: ↑ Kp
   - Si hay error estacionario: ↑ Ki (con cuidado)

---

## Ejemplos de comandos

### Secuencia básica completa

```bash
# 1. Activar modo (auto-homing)
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/mode" \
  -m '{"mode":"PID_POS"}'

# 2. Esperar a que llegue a home (verificar telemetría)
# ol_state debería ser "PID_READY"

# 3. Configurar PID y setpoint
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","kp":10,"ki":0.2,"kd":0.8,"setpoint":60,"zona_seguridad":5}'

# 4. Ejecutar control
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidpos" \
  -m '{"action":"run"}'

# 5. Detener si es necesario
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidpos" \
  -m '{"action":"stop"}'

# 6. Ir a home y repetir
mosquitto_pub -h 192.168.4.2 -t "pendulum/cmd/pidpos" \
  -m '{"action":"home"}'
```

### Comandos rápidos

```bash
# Ir a home
mosquitto_pub -t "pendulum/cmd/pidpos" -m '{"action":"home"}'

# Ir a end
mosquitto_pub -t "pendulum/cmd/pidpos" -m '{"action":"end"}'

# Stop de emergencia
mosquitto_pub -t "pendulum/cmd/stop" -m '{}'

# Solo cambiar setpoint (mantener otras config)
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","setpoint":30}'

# Ejecutar con parámetros agresivos
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","kp":15,"ki":0.05,"kd":1.0,"setpoint":70}'
mosquitto_pub -t "pendulum/cmd/pidpos" -m '{"action":"run"}'
```

### Monitoreo de telemetría

```bash
# Suscribirse a telemetría completa
mosquitto_sub -h 192.168.4.2 -t "pendulum/telemetry" -v

# Filtrar solo campos PID (con jq)
mosquitto_sub -h 192.168.4.2 -t "pendulum/telemetry" | \
  jq '{state: .ol_state, pos: .pos_mag_cm, error: .pid_error, output: .pid_output}'
```

---

## Troubleshooting

### Problema: El carro no se mueve

**Posibles causas:**
1. Estado incorrecto (no está en RUNNING)
2. Setpoint inválido (fuera de zona segura)
3. PWM muy bajo para vencer fricción estática

**Soluciones:**
```bash
# Verificar estado
mosquitto_sub -t "pendulum/telemetry" | jq '.ol_state'

# Verificar setpoint vs zona
# Si pid_setpoint < pid_zona_seg o > (90 - pid_zona_seg), es inválido

# Aumentar Kp para más fuerza
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","kp":15}'
```

### Problema: El carro oscila alrededor del setpoint

**Posibles causas:**
1. Kp muy alto
2. Kd muy bajo
3. Ki acumulando demasiado

**Soluciones:**
```bash
# Reducir Kp, aumentar Kd
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","kp":5,"kd":1.5}'

# Si sigue oscilando, reducir Ki
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","ki":0.05}'
```

### Problema: Error estacionario (nunca llega exacto)

**Posibles causas:**
1. Ki muy bajo o cero
2. Tolerancia muy estricta
3. Fricción estática mayor que error × Kp

**Soluciones:**
```bash
# Aumentar Ki gradualmente
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","ki":0.3}'

# O aumentar tolerancia
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","tolerancia":2.0}'
```

### Problema: Timeout de homing

**Posibles causas:**
1. Limit switch no funciona
2. Motor no tiene suficiente torque
3. Obstrucción mecánica

**Soluciones:**
1. Verificar conexión de limit switches
2. Verificar que `HOMING_PWM` (300) es suficiente
3. Revisar mecánica del sistema

### Problema: Se activa zona de seguridad muy pronto

**Posibles causas:**
1. `zona_seguridad` configurada muy grande
2. Setpoint muy cerca de límites

**Soluciones:**
```bash
# Reducir zona de seguridad (mínimo 2 cm)
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","zona_seguridad":3}'

# O mover setpoint más al centro
mosquitto_pub -t "pendulum/cmd/pidpos" \
  -m '{"action":"config","setpoint":45}'
```

---

## Problemas conocidos y soluciones

Esta sección documenta los problemas identificados durante las pruebas de integración HMI-ESP32 y las soluciones implementadas.

### [v2.0] Pérdida de datos por desconexiones WiFi

**Síntoma:** Gaps intermitentes en la telemetría CSV (hasta 47 segundos en casos extremos). Serial logs muestran `[mqttbr] Reconectando...` y backend reporta `[Errno 54] Connection reset by peer`.

**Causa raíz:** El modo WiFi AP del ESP32 es inherentemente inestable para conexiones de larga duración. El ESP32 actúa como Access Point y el Mac se conecta como cliente, pero la conexión WiFi se reinicia periódicamente.

**Soluciones implementadas:**

1. **MQTT Keepalive agresivo** (`MqttBridge.cpp`):
   ```cpp
   _wifiClient.setTimeout(5);     // Socket timeout 5s
   _mqttClient.setKeepAlive(5);   // Keepalive cada 5s
   ```

2. **Reconexión rápida** (`MqttBridge.cpp`):
   - Backoff: 500ms inicial → 5s máximo (antes: 1s → 30s)
   - Reintentos inmediatos tras desconexión

3. **Timeout de seguridad aumentado** (`main.cpp`):
   ```cpp
   #define PID_POSICION_SAFETY_TIMEOUT_MS 10000  // Antes: 5000ms
   ```
   Permite que el control PID continúe durante reconexiones breves.

**Resultado:** Reducción de ~60% en gaps de telemetría. El motor ya no se detiene por timeouts normales de reconexión.

### [v2.0] Saturación del backend HMI

**Síntoma:** Telemetría degrada progresivamente. Primeros 2 minutos: ~100ms entre muestras. Minuto 4+: gaps de 16-47 segundos. La base de datos SQLite y WebSocket acumulan backlog.

**Causa raíz:** `await registrar_telemetria()` bloqueaba el loop de MQTT mientras escribía a SQLite. WebSocket broadcast era secuencial, un cliente lento bloqueaba a todos.

**Soluciones implementadas:**

1. **Telemetría fire-and-forget** (`mqtt_cliente.py`):
   ```python
   # Antes (bloqueante)
   await registrar_telemetria(mensaje)
   
   # Después (no bloqueante)
   asyncio.create_task(registrar_telemetria(mensaje))
   ```

2. **Broadcast WebSocket paralelo** (`websocket_manager.py`):
   ```python
   await asyncio.gather(
       *[asyncio.wait_for(ws.send_json(data), timeout=0.5) 
         for ws in clients],
       return_exceptions=True
   )
   ```

3. **Batch settings optimizados** (`config.py`):
   - `REGISTRO_BATCH_SIZE`: 50 → 100
   - `REGISTRO_FLUSH_INTERVAL_S`: 2 → 5

4. **Frecuencia de telemetría reducida** (`hardware_pins.h`):
   - `TELEMETRY_INTERVAL_MS`: 100 → 200 (10 Hz → 5 Hz)

**Resultado:** Backend ya no se satura. Los gaps restantes son 100% atribuibles a desconexiones WiFi.

### [v2.0] Correlación de timestamps difícil

**Síntoma:** Logs del Serial Monitor no tenían timestamps, imposible correlacionar con datos CSV para debugging.

**Solución:** Implementado módulo de sincronización de tiempo:

**Nuevos archivos:**
- `SPISC/include/time_sync.h`
- `SPISC/src/time_sync.cpp`

**Funcionamiento:**
1. Backend envía timestamp ISO 8601 cada 10s a `pendulum/cmd/time_sync`
2. ESP32 parsea y calcula offset con `millis()`
3. Logs PID usan macro con timestamp sincronizado:
   ```
   [03:44:15.847][pidpos] Estado → PID_RUNNING
   ```

**Configuración MQTT:**
- Tópico: `pendulum/cmd/time_sync` (en `wifi_mqtt_config.h`)
- Formato: `{"timestamp": "2026-04-02T03:44:15.847Z"}`

### [v3.0] Gaps de telemetría durante movimiento del motor (500-1100ms)

**Síntoma:** Con telemetría a 5 Hz, durante PID_RUNNING con motor activo (PWM=700), los gaps entre muestras subían a 500-1100ms. En retrocesos (BWD), se recibían **CERO datos** durante todo el movimiento.

**Causa raíz:** EMI del motor de DC a alta potencia interfiere con la radio WiFi del ESP32. TCP se congestiona → `WiFiClient::write()` bloquea el thread de comunicaciones hasta que TCP retransmite → la publicación MQTT tarda cientos de ms.

**Soluciones implementadas (P0 + P1 + P2):**

1. **P0 — TCP Nagle deshabilitado** (`MqttBridge.cpp`):
   ```cpp
   _wifiClient.setNoDelay(true);  // En begin()
   ```
   Fuerza envío inmediato de cada paquete sin esperar a acumular datos.

   > **Nota:** Se evaluó `availableForWrite()` como check pre-publicación, pero retorna 0 en ESP32 WiFiClient, causando que TODAS las publicaciones sean descartadas silenciosamente. Se removió.

2. **P1 — Skip OLED durante telemetría fast** (`main.cpp`):
   ```cpp
   bool isFast = pkt.telemetry_fast;
   if (!isFast) {
       DisplayOLED::update(pkt);
   }
   lastDisplayMs = now;  // Siempre actualizar para evitar re-entry
   ```
   Libera ~5-10ms por ciclo de Core 1 al no actualizar el display I2C durante control activo.

3. **P2 — JSON compacto para modo fast** (`mqtt_comm.cpp`):
   ```cpp
   void publishTelemetryFast(const TelemetryPacket& p);
   // 19 campos esenciales, ~170 bytes vs ~500 bytes del modo normal
   ```
   Reduce payload 66%, minimizando tiempo en `WiFiClient::write()`.

4. **Frecuencia dinámica** (`main.cpp`):
   ```cpp
   #define TELEMETRY_FAST_INTERVAL_MS  50   // 20 Hz target durante PID activo
   #define TELEMETRY_INTERVAL_MS       200  // 5 Hz en reposo
   ```

5. **Buffer frontend aumentado** (`pendulumStore.js`):
   ```js
   const BUFFER_SIZE = 6000;  // Antes: 3000
   ```

**Resultado:** Gaps >200ms eliminados por completo. Gap mediano: 68ms (~14.7 Hz), P95: 76ms, P99: 93ms. Cero dropouts tanto en FWD como BWD.

### [v3.0] Posición del encoder IR congelada durante PID_RUNNING

**Síntoma:** `pos_ir_cm` siempre reportaba 0.00 durante PID_RUNNING, a pesar de que el conteo raw de pulsos (`pulsos_ir`) incrementaba correctamente (365→1267). Error PID se mantenía en 45.0 constantemente, PWM nunca subía de 361-369.

**Causa raíz:** En `encoder_ir.cpp`, la función `_updateGlobalPosition()` determinaba la dirección del movimiento **exclusivamente** a partir de la velocidad del sensor magnético AS5600 (`getMagVelocityCmPerSec()`). Con un umbral de 0.5 cm/s:

```cpp
// ANTES (v2) — Código problemático
if (velMag > VEL_THRESHOLD) {
    _globalPosition += absDelta;     // Solo si AS5600 detecta mov. adelante
} else if (velMag < -VEL_THRESHOLD) {
    _globalPosition -= absDelta;     // Solo si AS5600 detecta mov. atrás
}
// Si |velMag| < umbral → DESCARTA todos los pulsos como "ruido"
```

Como el AS5600 no reportaba movimiento (`rpm_mag=0.0` siempre), `velMag ≈ 0`, y **todos los pulsos del encoder IR eran descartados**. La posición global nunca cambiaba → `getPositionCm()` siempre retornaba 0.

**Consecuencia en cascada:**
1. `pos_ir` = 0 siempre → error = 45.0 siempre → PWM bajo (~361)
2. Carro se movía lento → auto-calibración LS_START resetea posición repetidamente
3. Safety timeout → stop de emergencia

**Solución:** Esquema de dirección con dos fuentes y prioridad (`encoder_ir.cpp`):

```cpp
// DESPUÉS (v3) — Esquema con prioridad
MotorDir motorDir = Motor::getDirection();
if (motorDir == DIR_ADELANTE) {
    _globalPosition += absDelta;     // Motor activo → usar su dirección
} else if (motorDir == DIR_ATRAS) {
    _globalPosition -= absDelta;
} else {
    // Motor parado → fallback a sensor magnético (mov. manuales)
    float velMag = SensorsI2C::getMagVelocityCmPerSec();
    if (velMag > VEL_THRESHOLD)      _globalPosition += absDelta;
    else if (velMag < -VEL_THRESHOLD) _globalPosition -= absDelta;
}
```

**Resultado:** Posición IR ahora cambia correctamente en ambas direcciones. Runs FWD: 0→44.5 cm. Runs BWD: 90→45.4 cm. Cada run converge en ~1.1s.

### [v3.0 — Pendiente] Sensor magnético AS5600 sin movimiento

**Síntoma:** `pos_mag_cm` y `rpm_mag` siempre en 0.0 (o valor de calibración). El sensor se comunica por I2C (se puede leer el ángulo raw) pero no detecta rotación del imán.

**Estado:** En investigación. Verificaciones de conexionado físico realizadas sin hallazgos. El firmware funciona correctamente con el encoder IR como sensor primario.

**Impacto:** Bajo. El control PID usa sensor IR con éxito. El fallback a velocidad magnética para dirección del encoder solo aplica cuando el motor está detenido (movimientos manuales), cubierto por el esquema de prioridad v3.

---

## Optimizaciones de telemetría (v3.0)

### Resumen de cambios por archivo

| Archivo | Cambio | Propósito |
|---------|--------|-----------|
| `hardware_pins.h` | `TELEMETRY_FAST_INTERVAL_MS = 50` | Intervalo 20 Hz target |
| `telemetry_data.h` | `bool telemetry_fast` en TelemetryPacket | Flag para selección de modo |
| `main.cpp` | `buildTelemetry()` setea flag; `taskComms()` usa intervalo dinámico | Orquestación |
| `main.cpp` | OLED skip cuando `telemetry_fast=true` | Liberar CPU Core 1 |
| `mqtt_comm.h/cpp` | `publishTelemetryFast()` | JSON compacto 19 campos |
| `encoder_ir.cpp` | Dirección por `Motor::getDirection()` con fallback magnético | Fix posición congelada |
| `MqttBridge.cpp` | `setNoDelay(true)` en begin() | Disable Nagle algorithm |
| `pendulumStore.js` | `BUFFER_SIZE = 6000` | Soporte para mayor tasa de datos |

### Métricas de rendimiento (prueba 03/04/2026)

| Métrica | v2.0 (antes) | v3.0 (después) |
|---------|-------------|-----------------|
| Frecuencia durante PID | 5 Hz (~200ms) | **14.7 Hz (~68ms)** |
| Gap máximo durante motor | 500-1100ms | **179ms** |
| Gaps >200ms | Frecuentes | **0** |
| Datos BWD (retroceso) | **CERO muestras** | **19-21 muestras/run** |
| Posición IR durante control | **Congelada en 0.0** | **Cambia correctamente** |
| Runs capturados (12 totales) | Parciales/incompletos | **12/12 completos** |
| Duración promedio de run | N/A (no convergía) | **~1.1-1.3s** |
| Payload promedio | ~500 bytes | **~170 bytes** (fast) |

---

## Estado actual del sistema

### Configuración de referencia (prueba exitosa 03/04/2026)

```
Kp=22.80  Ki=2.670  Kd=0.500
Setpoint=45.0 cm  Zona_Seg=5.0 cm  Tolerancia=0.5 cm
Sensor=IR  PWM=[300, 700]
```

### Qué funciona

- Control PID converge en ~1.1s tanto en FWD (0→45) como BWD (90→45)
- Telemetría fast a ~14.7 Hz sin dropouts durante movimiento del motor
- Encoder IR reporta posición correcta en ambas direcciones
- Homing automático a LS_START y LS_END funcionales
- Auto-calibración de posición en limit switches
- HMI muestra gráficas en tiempo real (posición, error, salida PID, integral, derivada)
- Exportación CSV con ~20 samples por run de ~1.1s

### Qué no funciona / limitaciones

- **AS5600 magnético**: No reporta movimiento (en investigación hardware)
- **Telemetría no alcanza 20 Hz exactos**: Real ~14.7 Hz (68ms mediano). Overhead de serialización JSON + TCP + WiFi AP impide alcanzar el target de 50ms
- **OLED apagado durante control**: No se ve feedback visual en el dispositivo mientras PID corre (trade-off intencional por rendimiento)

---

## Mejoras futuras propuestas

### 1. Ring buffer en firmware (Prioridad: Media)

Almacenar telemetría localmente cuando MQTT está desconectado o TCP bloqueado, y enviar al recuperar la conexión. Con la telemetría fast (P2), los dropouts son mínimos, pero un ring buffer daría cobertura completa.

```cpp
#define RING_BUFFER_SIZE 500  // ~25 segundos a 20 Hz
CircularBuffer<TelemetryPacket, RING_BUFFER_SIZE> telemetryBuffer;
```

### 2. Diagnóstico del sensor AS5600 (Prioridad: Alta)

Resolver el problema de hardware/conexionado que impide al sensor magnético reportar movimiento. Una vez funcional:
- Habilitaría fusión de sensores IR+magnético para mayor precisión
- Restauraría el fallback magnético para detección de movimientos manuales
- Permitiría usar `sensor: "mag"` como sensor primario (mayor resolución)

### 3. WiFi modo Station (Prioridad: Media)

Cambiar de AP mode a Station mode conectándose a un router existente. Reduciría interferencia y mejoraría estabilidad WiFi. Podría acercar la telemetría al target de 20 Hz.

### 4. ~~Telemetría adaptativa~~ → Implementada como telemetría dinámica (v3.0)

~~Ajustar frecuencia dinámicamente según calidad de conexión.~~

Implementada de forma simplificada: 20 Hz durante PID activo, 5 Hz en reposo. Una versión más sofisticada podría ajustar basándose en packet loss medido.

### 5. MQTT QoS 1 (Prioridad: Baja)

Con el JSON compacto y Nagle deshabilitado, la pérdida de paquetes es mínima. QoS 1 agregaría overhead sin beneficio significativo actual.

### 6. Protocolo UDP para telemetría (Prioridad: Baja)

Evaluar UDP broadcast para eliminar overhead TCP. Beneficio marginal dado que los gaps actuales son aceptables (~68ms mediano).

---

## Referencias

- [Modo Lazo Abierto](../SPISC/src/modes/lazo_abierto_mode.cpp) — Lógica de homing reutilizada
- [Calibración Sensor Magnético](calibracion_posicion_mag.md) — Calibración del AS5600
- [Arquitectura Dual-Core](config_dual_core.md) — Ejecución en Core 0/1
