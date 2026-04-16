# Análisis de `mqtt_cliente.py`

## Contexto

`mqtt_cliente.py` es el **puente de comunicación** entre el broker MQTT (Mosquitto) y el backend FastAPI del HMI. Se ubica en la capa intermedia de la arquitectura:

```
ESP32  ──MQTT──►  Mosquitto  ──MQTT──►  mqtt_cliente.py  ──WS──►  Frontend Vue.js
                                              │
                                              ▼
                                        SQLite (registro)
```

Su rol principal es:
- **Recibir** telemetría, estado y eventos del ESP32 vía MQTT.
- **Reenviar** esos datos al frontend a través del WebSocket manager.
- **Persistir** datos en SQLite a través del módulo de registro.
- **Publicar** comandos del usuario hacia el ESP32.
- **Sincronizar** el reloj del ESP32 con el servidor.

---

## Dependencias

| Paquete | Uso |
|---------|-----|
| `aiomqtt` | Cliente MQTT asíncrono (wrapper de paho-mqtt sobre asyncio) |
| `orjson` | Serialización/deserialización JSON de alto rendimiento |
| `config` | Módulo interno con hosts, puertos y nombres de tópicos MQTT |
| `GestorWebSocket` | Broadcast de datos hacia los clientes WebSocket (frontend) |
| `RegistroDatos` | Persistencia asíncrona en SQLite |

Las dependencias `GestorWebSocket` y `RegistroDatos` se importan condicionalmente con `TYPE_CHECKING` para evitar importaciones circulares y se inyectan en tiempo de ejecución.

---

## Estructura de la clase `ClienteMQTT`

### 1. `__init__` — Inicialización

```python
def __init__(self) -> None:
    self._gestor_ws    = None   # Referencia al gestor WebSocket
    self._registro     = None   # Referencia al registro de datos (SQLite)
    self._client       = None   # Instancia de aiomqtt.Client activa
    self._ultimo_estado      = None   # Cache del último estado recibido
    self._ultima_telemetria  = None   # Cache de la última telemetría
    self._ultimos_limites    = None   # Cache de los últimos límites
    self._tarea        = None   # asyncio.Task del loop principal
```

Se crean atributos internos sin conexión real. Los caches (`_ultimo_*`) permiten que los endpoints REST devuelvan el último valor conocido sin esperar un nuevo mensaje MQTT.

### 2. `vincular()` — Inyección de dependencias

```python
def vincular(self, gestor_ws, registro) -> None
```

Conecta las dependencias externas (`GestorWebSocket` y `RegistroDatos`) al cliente. Este patrón desacopla la construcción del objeto de su configuración, permitiendo que `app.py` cree las instancias en el orden correcto y luego las enlace.

### 3. `iniciar()` / `detener()` — Ciclo de vida

- **`iniciar()`**: Crea un `asyncio.Task` que ejecuta `_loop_conexion()` en background. Se invoca durante el evento `startup` de FastAPI.
- **`detener()`**: Cancela la tarea y espera su finalización. Se invoca durante el evento `shutdown` de FastAPI.

### 4. `_loop_conexion()` — Loop principal con reconexión automática

Este es el corazón del módulo. Su flujo es:

```
┌─────────────────────────────────────────────┐
│  while True:                                │
│    1. Conectar al broker MQTT               │
│    2. Suscribirse a "pendulum/#"            │
│    3. Iniciar tarea de time_sync            │
│    4. Iterar mensajes entrantes             │
│       └─ _procesar_mensaje() por cada uno   │
│                                             │
│  Si la conexión falla:                      │
│    - Log del error                          │
│    - Esperar con backoff exponencial        │
│      (1s → 2s → 4s → ... → máx 30s)       │
│    - Reintentar                             │
└─────────────────────────────────────────────┘
```

**Detalles clave:**
- Usa `aiomqtt.Client` como context manager (`async with`), lo que garantiza desconexión limpia.
- El `identifier="hmi-backend"` permite identificar esta conexión en el broker.
- Al reconectarse exitosamente, resetea el backoff a 1 segundo.
- `asyncio.CancelledError` rompe el loop (señal de shutdown).
- La referencia `self._client` se establece en `None` en el `finally` para reflejar desconexión.

### 5. `_procesar_mensaje()` — Router de mensajes entrantes

Recibe cada mensaje MQTT y lo enruta según el tópico:

| Tópico | Acción |
|--------|--------|
| `pendulum/telemetry` | Cachea → broadcast WS (`"telemetria"`) → registra en SQLite (fire-and-forget) |
| `pendulum/status` | Cachea → broadcast WS (`"estado"`) → registra evento (await) |
| `pendulum/limits` | Cachea → broadcast WS (`"limites"`) → registra evento (await) |
| `pendulum/sweep/results` | Broadcast WS (`"sweep_results"`) → registra evento (await) |

**Patrón fire-and-forget**: Para telemetría (alta frecuencia), tanto el broadcast WS como el registro en SQLite se lanzan con `asyncio.create_task()` sin `await`, evitando que bloqueen el procesamiento del siguiente mensaje MQTT. Para estado/límites (baja frecuencia), el registro se hace con `await` directo.

Si el payload no es JSON válido, se ignora con un log de debug.

### 6. `_loop_time_sync()` — Sincronización de reloj

```
Cada 10 segundos:
  → Publica {"ts": <epoch_ms>} en "pendulum/time"
```

El ESP32 usa este timestamp para sincronizar su reloj interno (no tiene RTC). Se ejecuta como tarea paralela dentro del contexto de conexión y se cancela si la conexión se pierde.

### 7. `publicar()` — Envío de comandos al ESP32

```python
async def publicar(self, topico: str, payload: dict) -> bool
```

Serializa `payload` a JSON con `orjson` y lo publica en el tópico indicado. Retorna `True` si se envió, `False` si no hay conexión o hay error. Es el método que usan los endpoints REST/WS del backend para enviar comandos como:
- `pendulum/cmd/mode` — cambiar modo de operación
- `pendulum/cmd/stop` — detener motor
- `pendulum/cmd/pidpos` — configurar PID de posición
- etc.

### 8. Propiedades (snapshots REST)

```python
@property estado_actual     → dict | None
@property telemetria_actual → dict | None
@property limites_actual    → dict | None
@property conectado         → bool
```

Exponen los valores cacheados para que los endpoints REST (`GET /api/estado`, etc.) puedan responder inmediatamente sin suscribirse a MQTT. `conectado` indica si hay una conexión activa al broker.

---

## Flujo completo de un dato (ejemplo: telemetría)

```
1. ESP32 publica JSON en "pendulum/telemetry" vía WiFi/MQTT
2. Mosquitto (broker) enruta el mensaje a todos los suscriptores
3. _loop_conexion() recibe el mensaje en el `async for`
4. _procesar_mensaje() identifica el tópico como TOPICO_TELEMETRIA
5. Cachea el dato en self._ultima_telemetria
6. Lanza broadcast WS → todos los navegadores conectados reciben el dato
7. Lanza registro SQLite → el dato se acumula en buffer y se graba por lotes
```

## Flujo completo de un comando (ejemplo: cambiar modo)

```
1. Usuario hace clic en "PID Posición" en el frontend Vue.js
2. Frontend envía {"accion": "modo", "valor": "pid_pos"} por WebSocket
3. GestorWebSocket procesa el mensaje y llama:
   await cliente_mqtt.publicar("pendulum/cmd/mode", {"mode": "pid_pos"})
4. publicar() serializa con orjson y envía al broker
5. Mosquitto enruta al ESP32
6. ESP32 cambia su modo de operación
```

---

## Patrones de diseño utilizados

| Patrón | Implementación |
|--------|---------------|
| **Inyección de dependencias** | `vincular()` conecta colaboradores sin acoplar constructores |
| **Reconexión con backoff exponencial** | Loop infinito con espera creciente (1→2→4→...→30s) |
| **Fire-and-forget** | `asyncio.create_task()` para operaciones no bloqueantes |
| **Cache en memoria** | `_ultimo_*` para respuestas REST inmediatas |
| **Context manager** | `async with aiomqtt.Client(...)` para conexión limpia |
| **TYPE_CHECKING guard** | Evita importaciones circulares entre módulos del backend |
