# Control PID de Posición del Carro

**Proyecto:** [SPISC — Sistema de Péndulo Invertido con Supervisión y Control](../README.md)  
**Fecha:** 01/04/2026  
**Última actualización:** 02/04/2026 (v2.1)  
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
- [Mejoras futuras propuestas](#mejoras-futuras-propuestas)
- [Referencias](#referencias)
- [Parámetros configurables](#parámetros-configurables)
- [Algoritmo PID](#algoritmo-pid)
- [Zonas de seguridad](#zonas-de-seguridad)
- [Telemetría extendida](#telemetría-extendida)
- [Guía de uso](#guía-de-uso)
- [Ejemplos de comandos](#ejemplos-de-comandos)
- [Troubleshooting](#troubleshooting)

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
│   (AS5600 magnético)                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Características principales

| Característica | Valor por defecto | Rango configurable |
|---------------|-------------------|---------------------|
| **Frecuencia de control** | 1000 Hz (Core 0) | Fijo |
| **Sensor de posición** | AS5600 magnético | `mag` / `ir` |
| **Rango de posición** | 0 - 90 cm | Fijo |
| **PWM mínimo** | 200 | [100, 500] |
| **PWM máximo** | 700 | [300, 1023] |
| **Anti-windup** | Saturación de integral ±500 | Fijo |
| **Filtro derivativo** | Pasa-bajos α = 0.2 | Fijo |

> **Nota v2**: El sensor y límites PWM son configurables vía MQTT.

---

## Arquitectura del modo

### Archivos involucrados

```
SPISC/
├── src/modes/
│   ├── pid_posicion_mode.h     ← Header con interfaz pública
│   └── pid_posicion_mode.cpp   ← Implementación del modo
├── include/
│   ├── telemetry_data.h        ← Campos PID en TelemetryPacket
│   └── wifi_mqtt_config.h      ← TOPIC_CMD_PID_POS
└── src/
    ├── main.cpp                ← Registro del modo
    └── mqtt_comm.cpp           ← Serialización telemetría PID
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
            └─────◄───── AS5600 ◄───── Posición ◄───── Carro        │
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

### Campos PID en telemetría

El modo agrega los siguientes campos al paquete de telemetría (`pendulum/telemetry`):

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `pid_error` | float | Error actual (setpoint - pos_actual) en cm |
| `pid_integral` | float | Término integral acumulado |
| `pid_derivada` | float | Término derivativo filtrado |
| `pid_output` | float | Salida cruda del controlador (antes de saturación) |
| `pid_output_sat` | float | Salida saturada (después de clamp a [pwm_min, pwm_max]) |
| `pid_setpoint` | float | Setpoint configurado en cm |
| `pid_kp` | float | Ganancia proporcional actual |
| `pid_ki` | float | Ganancia integral actual |
| `pid_kd` | float | Ganancia derivativa actual |
| `pid_zona_seg` | float | Zona de seguridad en cm |
| `pid_tolerancia` | float | Tolerancia de llegada en cm |
| `pid_sensor_type` | int | Sensor activo: 0=magnético, 1=IR |
| `pid_pwm_min` | int | PWM mínimo configurado |
| `pid_pwm_max` | int | PWM máximo configurado |

> **Nota v2**: `pid_output` muestra el valor crudo del PID; `pid_output_sat` muestra el valor aplicado al motor después del anti-windup y saturación.

### Ejemplo de telemetría

```json
{
  "pos_mag_cm": 42.35,
  "pwm": 450,
  "direction": 1,
  "ol_state": "PID_RUNNING",
  "modo": "PID_POS",
  "pid_error": 2.65,
  "pid_integral": 125.3,
  "pid_derivada": -15.2,
  "pid_output": 478.5,
  "pid_output_sat": 450.0,
  "pid_setpoint": 45.0,
  "pid_kp": 8.0,
  "pid_ki": 0.1,
  "pid_kd": 0.5,
  "pid_zona_seg": 5.0,
  "pid_tolerancia": 1.0,
  "pid_sensor_type": 0,
  "pid_pwm_min": 200,
  "pid_pwm_max": 700
}
```

> En este ejemplo, `pid_output=478.5` (crudo) fue saturado a `pid_output_sat=450.0` debido al límite `pwm_max=700` aplicado con clamp.

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

Esta sección documenta los problemas identificados durante las pruebas de integración HMI-ESP32 y las soluciones implementadas (sesión de debugging 02/04/2026).

### Pérdida de datos por desconexiones WiFi

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

### Saturación del backend HMI

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

### Correlación de timestamps difícil

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

---

## Mejoras futuras propuestas

Las siguientes mejoras se proponen para resolver los problemas de conectividad WiFi que persisten:

### 1. Ring buffer en firmware (Prioridad: Alta)

Almacenar telemetría localmente cuando MQTT está desconectado y enviar el buffer al reconectar.

```cpp
// Pseudocódigo propuesto
#define RING_BUFFER_SIZE 500  // ~100 segundos a 5 Hz
CircularBuffer<TelemetryPacket, RING_BUFFER_SIZE> telemetryBuffer;

void sendTelemetry() {
    if (mqttConnected) {
        // Enviar buffer acumulado primero
        while (!telemetryBuffer.isEmpty()) {
            publish(telemetryBuffer.shift());
        }
        // Luego telemetría actual
        publish(currentTelemetry);
    } else {
        // Guardar para después
        telemetryBuffer.push(currentTelemetry);
    }
}
```

### 2. MQTT QoS 1 con ACK (Prioridad: Media)

Actualmente se usa QoS 0 (fire-and-forget). QoS 1 garantiza entrega con acknowledgment.

**Trade-off:** Mayor overhead de red, pero garantiza que cada paquete llegue al menos una vez.

### 3. WiFi modo Station (Prioridad: Alta)

Cambiar de AP mode a Station mode conectándose a un router existente:

**Ventajas:**
- Conexión WiFi más estable
- ESP32 no tiene que manejar clientes
- Posibilidad de Internet para NTP real

**Desventajas:**
- Requiere router disponible
- Configuración de red previa

### 4. Protocolo UDP para telemetría (Prioridad: Baja)

Reemplazar MQTT por UDP broadcast para telemetría de alta frecuencia:

**Ventajas:**
- Sin overhead de TCP handshake
- Sin colas de MQTT
- Latencia mínima

**Desventajas:**
- No hay garantía de entrega
- Implementación custom
- MQTT sigue necesario para comandos

### 5. Telemetría adaptativa (Prioridad: Media)

Ajustar frecuencia de telemetría dinámicamente según calidad de conexión:

```cpp
// Pseudocódigo propuesto
void adjustTelemetryRate() {
    float packetLoss = getRecentPacketLoss();
    if (packetLoss > 0.2) {
        telemetryInterval = min(telemetryInterval * 1.5, 500);  // Más lento
    } else if (packetLoss < 0.05) {
        telemetryInterval = max(telemetryInterval * 0.8, 100);  // Más rápido
    }
}
```

---

## Referencias

- [Modo Lazo Abierto](../SPISC/src/modes/lazo_abierto_mode.cpp) — Lógica de homing reutilizada
- [Calibración Sensor Magnético](calibracion_posicion_mag.md) — Calibración del AS5600
- [Arquitectura Dual-Core](config_dual_core.md) — Ejecución en Core 0/1
