# Control PID Cascada — Péndulo Invertido

## Descripción General

El modo **PID Cascada** implementa un control en lazo cerrado con dos controladores PID encadenados:

```
           ┌──────────────────┐         ┌──────────────────┐
pos_sp ──→ │  PID Posición    │──θ_ref──→│  PID Ángulo      │──→ PWM → Motor
           │  (lazo externo)  │         │  (lazo interno)  │
           └──────────────────┘         └──────────────────┘
                 ↑                            ↑
            pos_actual                   angle_actual
```

- **Lazo externo (Posición)**: Calcula el error de posición del carro (setpoint - pos_actual) y genera una referencia angular `θ_ref`, saturada a ±`theta_ref_max` (por defecto ±2°).
- **Lazo interno (Ángulo)**: Usa `θ_ref` como setpoint angular y aplica el PID de ángulo para generar la señal PWM que controla el motor.

> Se utiliza el controlador de posición para "engañar" al controlador de ángulo, inclinando ligeramente el péndulo para que el carro se desplace hacia la posición deseada.

## Máquina de Estados

```
PIDC_IDLE → PIDC_HOMING_START → PIDC_CALIBRATING (200ms)
         → PIDC_WAIT_READY (2s) → PIDC_READY
         → (usuario presiona EJECUTAR) → PIDC_RUNNING
         → (detener o fuera de rango) → PIDC_STOPPED
```

| Estado | Descripción |
|---|---|
| `PIDC_IDLE` | Modo inactivo, sin control |
| `PIDC_HOMING_START` | Motor moviendose hacia el inicio del riel |
| `PIDC_HOMING_END` | Motor moviendose hacia el final del riel |
| `PIDC_CALIBRATING` | Calibración de sensores (200ms) |
| `PIDC_WAIT_READY` | Espera de 2 segundos antes de habilitar control |
| `PIDC_READY` | Listo para ejecutar (requiere acción del usuario) |
| `PIDC_RUNNING` | Control cascada activo, ambos lazos ejecutándose |
| `PIDC_STOPPED` | Detenido (por usuario o por salir del rango de trabajo) |

**Diferencias con PID Ángulo:**
- `PIDC_WAIT_READY`: Retardo de 2s para estabilizar antes de habilitar
- `PIDC_READY → PIDC_RUNNING`: Requiere que el usuario presione "Ejecutar" (sin auto-transición)
- Fuera de rango → `PIDC_STOPPED` (sin reposicionamiento automático)

## Parámetros Configurables

### Lazo Externo (Posición → θ_ref)

| Parámetro | Rango | Default | Descripción |
|---|---|---|---|
| `kp_pos` | 0 – 20 | 0.5 | Ganancia proporcional posición |
| `ki_pos` | 0 – 5 | 0.01 | Ganancia integral posición |
| `kd_pos` | 0 – 5 | 0.1 | Ganancia derivativa posición |
| `pos_setpoint` | 5 – 85 cm | 45 cm | Posición objetivo del carro |
| `zona_seguridad` | 2 – 20 cm | 5 cm | Margen de seguridad en bordes del riel |
| `theta_ref_max` | 0.5 – 10° | 2° | Saturación de la referencia angular |

### Lazo Interno (Ángulo → PWM)

| Parámetro | Rango | Default | Descripción |
|---|---|---|---|
| `kp_ang` | 0.1 – 100 | 40 | Ganancia proporcional ángulo |
| `ki_ang` | 0 – 50 | 1 | Ganancia integral ángulo |
| `kd_ang` | 0 – 10 | 0.5 | Ganancia derivativa ángulo |
| `rango_trabajo` | 5 – 30° | 20° | Rango angular para control activo |
| `tolerancia_ang` | 0 – 5° | 1° | Tolerancia angular para "vertical" |

### Motor

| Parámetro | Rango | Default | Descripción |
|---|---|---|---|
| `pwm_min` | 100 – 500 | 250 | PWM mínimo (zona muerta) |
| `pwm_max` | 300 – 1023 | 900 | PWM máximo permitido |

## Comunicación MQTT

### Tópico de Comandos

`pendulum/cmd/pidcasc`

### Comandos JSON

```json
// Homing al inicio
{"action": "home"}

// Homing al final
{"action": "end"}

// Configurar parámetros
{"action": "config", 
  "kp_pos": 0.5, "ki_pos": 0.01, "kd_pos": 0.1,
  "pos_setpoint": 45, "zona_seguridad": 5, "theta_ref_max": 2,
  "kp_ang": 40, "ki_ang": 1, "kd_ang": 0.5,
  "rango_trabajo": 20, "tolerancia_ang": 1,
  "pwm_min": 250, "pwm_max": 900}

// Ejecutar control
{"action": "run"}

// Detener
{"action": "stop"}
```

### Telemetría (prefijo `pidc_`)

Campos publicados en `pendulum/telemetry`:

| Campo | Descripción |
|---|---|
| `pidc_pos_error` | Error de posición (cm) |
| `pidc_pos_integral` | Integral acumulada de posición |
| `pidc_pos_derivada` | Derivada de posición |
| `pidc_pos_output` | Salida cruda PID posición |
| `pidc_pos_output_sat` | θ_ref saturada (°) |
| `pidc_pos_setpoint` | Setpoint posición (cm) |
| `pidc_pos_kp/ki/kd` | Ganancias activas posición |
| `pidc_pos_zona_seg` | Zona de seguridad activa (cm) |
| `pidc_ang_error` | Error angular (°) |
| `pidc_ang_integral` | Integral acumulada angular |
| `pidc_ang_derivada` | Derivada angular |
| `pidc_ang_output` | Salida cruda PID ángulo |
| `pidc_ang_output_sat` | Salida saturada PID ángulo |
| `pidc_ang_setpoint` | Setpoint angular = θ_ref (°) |
| `pidc_ang_kp/ki/kd` | Ganancias activas ángulo |
| `pidc_rango_trabajo` | Rango de trabajo activo (°) |
| `pidc_tolerancia_ang` | Tolerancia angular activa (°) |
| `pidc_pwm_min/max` | Limites PWM activos |
| `pidc_theta_ref_max` | Límite θ_ref activo (°) |

## Archivos del Proyecto

### Firmware (SPISC/)

- `src/modes/pid_cascada_mode.h` — Header con API del namespace
- `src/modes/pid_cascada_mode.cpp` — Implementación completa (~500 líneas)
- `include/telemetry_data.h` — Enum `MODE_PID_CASC` + campos telemetría
- `include/wifi_mqtt_config.h` — Tópico MQTT
- `src/main.cpp` — Integración en dual-core
- `src/mqtt_comm.cpp` — Serialización JSON telemetría

### Backend (hmi/backend/)

- `config.py` — `TOPICO_CMD_PID_CASC`
- `websocket_manager.py` — Mapeo WebSocket → MQTT

### Frontend (hmi/frontend/src/)

- `stores/pendulumStore.js` — Estado reactivo + acciones
- `components/tabs/TabControlPidCascada.vue` — Tab principal de control
- `components/ResultadosPidCascada.vue` — Tabla de resultados + CSV
- `components/GraficosTelemetria.vue` — 4 gráficos cascada
- `App.vue` — Ruta del tab
- `components/PaginaInicio.vue` — Tarjeta del experimento

## Notas de Implementación

1. **Frecuencia de control**: 1000 Hz (ambos lazos se ejecutan en el mismo ciclo)
2. **Core 0**: Sensores + control (alta prioridad)
3. **Core 1**: Comunicación MQTT (baja prioridad)
4. **Thread safety**: Flags `volatile` para comandos cross-core, patrón pending config
5. **Anti-windup**: Reset de integrales al cambiar de estado o detener
6. **Normalización angular**: Error normalizado a ±180° en lazo interno
7. **Motor bias**: Compensación de zona muerta con `pwm_min` como offset base
