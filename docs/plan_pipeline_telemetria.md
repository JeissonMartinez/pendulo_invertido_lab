# Análisis y Plan de Optimización — Pipeline de Telemetría

> **Fecha:** 2026-04-16  
> **Problema:** Los paquetes de telemetría visualizados en la HMI se acumulan progresivamente. Lo que muestra la interfaz va "retardado" respecto al comportamiento real del prototipo. Esto no afecta el ciclo de control del firmware, pero dificulta la depuración en tiempo real.

---

## 1. Mapa del Pipeline

```
ESP32 Core 0 (1kHz)     ESP32 Core 1          Mosquitto       Backend Python        Frontend Vue.js
┌──────────────┐      ┌──────────────┐      ┌──────────┐    ┌──────────────┐     ┌────────────────┐
│ Sensores +   │mutex │ publishTelem │ QoS0 │          │MQTT│ aiomqtt loop │ WS  │ onmessage      │
│ Control PID  │─────►│ 5Hz / 20Hz   │─────►│  :1883   │───►│ create_task  │────►│ JSON.parse     │
│ 1ms loop     │      │ ~1400B/~800B │      │          │    │ broadcast()  │     │ procesarMsg()  │
└──────────────┘      │              │      └──────────┘    │ orjson.dumps │     │ 80+ fields     │
                      │ OLED ~92ms   │                      │ send_text()  │     │ 40+ arr.push() │
                      │ (bloqueante) │                      │ timeout 0.5s │     │ splice trim    │
                      └──────────────┘                      └──────────────┘     └────────────────┘
```

---

## 2. Análisis por Nodo

### 2.1 ESP32 Firmware (SPISC)

**Archivos clave:** `SPISC/src/main.cpp`, `SPISC/src/mqtt_comm.cpp`, `SPISC/lib/MqttBridge/MqttBridge.cpp`

| Parámetro | Valor |
|-----------|-------|
| Tasa normal | 5 Hz (200ms) — `TELEMETRY_INTERVAL_MS` |
| Tasa rápida (PID activo) | 20 Hz (50ms) — `TELEMETRY_FAST_INTERVAL_MS` |
| Tamaño JSON normal | ~1400 bytes (70+ campos, incluye TODOS los PID) |
| Tamaño JSON "fast" | ~800 bytes (aún incluye PID cascada completo) |
| QoS | 0 (fire-and-forget) |
| Nagle | Desactivado (`setNoDelay(true)`) |
| Buffer PubSubClient | 2048 bytes |
| OLED en Core 1 | ~92ms I2C blocking (desactivado en modo fast) |
| Comm task yield | 10ms (`vTaskDelay`) |

**Hallazgos:**

- `publishTelemetryFast()` no es realmente compacto — crece continuamente con cada nuevo modo PID agregado. Actualmente incluye **todos los campos de PID cascada** incluso cuando el modo activo es PID posición.
- A 20 Hz con ~800 bytes = **16 KB/s** en WiFi. Trivial en ancho de banda, pero no en serialización: el `snprintf()` con 60+ conversiones float toma tiempo en el ESP32.
- El `vTaskDelay(10ms)` en Core 1 significa que en el **peor caso**, la telemetría se envía 10ms después del instante programado. No es acumulativo.
- **OLED bloqueante:** En modo no-PID, `DisplayOLED::update()` comparte Core 1 con MQTT y bloquea ~92ms cada 500ms. No causa acumulación pero sí jitter en la entrega.

**Veredicto:** El ESP32 **no** es el cuello de botella. Publica de forma determinística y sin colas crecientes.

---

### 2.2 Mosquitto Broker

**Archivo:** `hmi/mosquitto/mosquitto.conf`

Configuración mínima: `listener 1883`, `allow_anonymous true`, `persistence true`. Sin tuning de QoS 0. Para QoS 0, Mosquitto actúa como pass-through: el mensaje entra y sale del broker en el mismo ciclo del event loop. Latencia añadida: **<1ms**.

**Veredicto:** Mosquitto **no** es el cuello de botella.

---

### 2.3 Backend Python (FastAPI + aiomqtt)

**Archivos clave:** `hmi/backend/mqtt_cliente.py`, `hmi/backend/websocket_manager.py`, `hmi/backend/registro_datos.py`

#### 2.3.1 Recepción MQTT → Despacho

```python
# mqtt_cliente.py — loop principal
async for msg in client.messages:     # ← SECUENCIAL: un mensaje a la vez
    await self._procesar_mensaje(msg)
```

Para telemetría, el despacho es **fire-and-forget**:

```python
asyncio.create_task(self._gestor_ws.broadcast({...}))       # no bloquea
asyncio.create_task(self._registro.registrar_telemetria(datos))  # no bloquea
```

Pero para `estado` y `limites` hay un **`await`** bloqueante:

```python
# mqtt_cliente.py líneas ~117, ~123
await self._registro.registrar_evento("estado", datos)  # ← BLOQUEA el loop MQTT
```

Cuando `registrar_evento` dispara un flush SQLite (`executemany` + `commit` con fsync), puede tomar **10-100ms**, durante los cuales los mensajes de telemetría se encolan en el buffer de aiomqtt.

#### 2.3.2 WebSocket Broadcast

```python
# websocket_manager.py
async def broadcast(self, mensaje: dict) -> None:
    datos_text = orjson.dumps(mensaje).decode("utf-8")  # serializa una vez
    # enviar a todos en paralelo con timeout 0.5s
    await asyncio.gather(*[enviar_seguro(ws) for ws in self._conexiones])
```

- Serialización `orjson`: ~0.1ms para 1400 bytes → insignificante.
- `send_text()` con timeout de 0.5s: si el browser tiene backpressure en TCP, el send se bloquea. Pero como cada broadcast es un `create_task` independiente, no bloquea el loop MQTT.
- **No hay control de backpressure ni dropping:** el backend genera un task por cada mensaje sin límite. Si el frontend no consume, los tasks se acumulan en el event loop de asyncio.

#### 2.3.3 Registro SQLite

`registrar_telemetria` usa un buffer batch (100 registros / 5s flush). A 20 Hz, el buffer se llena cada **5 segundos**, disparando:

```python
await self._db.executemany(...)  # 100 INSERTs
await self._db.commit()          # fsync al disco
```

Corre en su propio task, no bloquea el broadcast, pero compite por CPU en el event loop.

**Veredicto:** El backend contribuye con latencia intermitente (bloqueo de `registrar_evento` en estado) pero **no** es el origen de la acumulación progresiva.

---

### 2.4 Frontend Vue.js (CUELLO DE BOTELLA PRINCIPAL)

**Archivos clave:** `hmi/frontend/src/composables/useWebSocket.js`, `hmi/frontend/src/stores/pendulumStore.js`

#### 2.4.1 Procesamiento por mensaje — Carga de trabajo

Cada mensaje de telemetría que llega al `onmessage` del WebSocket ejecuta **sincrónicamente**:

| Operación | Costo estimado | Frecuencia |
|-----------|----------------|------------|
| `JSON.parse()` de ~800-1400 bytes | ~0.1ms | Por mensaje |
| `_normalizarTelemetria()` — objeto con 80+ campos y null-coalescing | ~0.2ms | Por mensaje |
| `telemetria.value = norm` — trigger reactivity en Vue Proxy | ~0.5ms | Por mensaje |
| `_agregarAlHistorial()` — **40+ `.push()` en arrays reactivos** | **~2-5ms** | Por mensaje |
| Sincronización de estado PID (8-15 asignaciones reactivas) | ~0.3ms | Por mensaje |
| `pidXxxResults.value.push({...datos})` — spread de 80+ campos | ~0.5ms | Por mensaje |
| Ring buffer trim `splice(0, exceso)` en **40+ arrays** | **~5-15ms** | Cuando buffer lleno |

**Estimación conservadora por mensaje:** ~4-6ms (sin trim), ~10-20ms (con trim)

**Budget disponible a 20 Hz:** 50ms entre mensajes. Suena como que hay margen, pero…

#### 2.4.2 El problema real: acumulación reactiva

Cada `.push()` en un array reactivo de Vue 3 (Proxy-based) dispara:

1. El set trap del Proxy intercepta la mutación.
2. Vue registra que la propiedad `length` cambió.
3. Ejecuta las dependencias registradas (watchers, computed).

Con **40 arrays reactivos × 20 Hz = 800 operaciones reactivas/segundo**, más los watchers del componente `GraficosTelemetria.vue` que observan `store.historial.ts.length`, el event loop de JavaScript se satura. El navegador prioriza las tareas de rendering (RAF) y los microtasks de Vue, pero el WebSocket `onmessage` queda en cola.

#### 2.4.3 El splice catastrófico

Una vez que el historial alcanza `BUFFER_SIZE = 6000` (a 20 Hz, en **5 minutos**):

```javascript
if (h.ts.length > BUFFER_SIZE) {
  const exceso = h.ts.length - BUFFER_SIZE
  for (const key of Object.keys(h)) {
    h[key].splice(0, exceso)  // ← O(n) shift + reactivity re-track
  }
}
```

`Array.splice(0, n)` en un array reactivo:

- Mueve `6000 - n` elementos hacia el inicio.
- Triggers la reactividad de Vue para todo el array.
- Se ejecuta **40+ veces** (una por cada serie del historial).
- Ocurre **en cada mensaje** una vez lleno el buffer.

Esto puede tomar **10-20ms por mensaje**, dejando solo ~30-40ms de los 50ms de budget, sin contar el resto del procesamiento.

#### 2.4.4 La latencia no se mide

```javascript
// useWebSocket.js línea ~50
latencia.value = Date.now() % 100  // placeholder visual — NO mide latencia real
```

No hay instrumentación real para detectar el retardo.

**Veredicto:** El frontend es el **cuello de botella principal**. La combinación de:

1. Alta tasa de mutaciones reactivas (40+ push por mensaje × 20 Hz).
2. El `splice` destructivo en 40+ arrays cada tick una vez lleno el buffer.
3. Cero throttling/batching en la recepción de WebSocket.

Produce una situación donde el **consumidor (JS event loop) es ligeramente más lento que el productor (20 Hz)**. La diferencia se acumula linealmente: si procesar un mensaje toma ~55ms promedio en lugar de 50ms, después de 1 minuto hay un retraso de **~12 segundos**.

---

## 3. Resumen de Cuellos de Botella (Ordenados por Impacto)

| # | Ubicación | Problema | Impacto |
|---|-----------|----------|---------|
| **1** | Frontend: `_agregarAlHistorial()` | 40+ `.push()` reactivos por mensaje | **CRÍTICO** — causa acumulación progresiva |
| **2** | Frontend: ring buffer `splice()` | O(n) × 40 arrays cuando buffer lleno | **ALTO** — empeora después de 5 min |
| **3** | Frontend: `procesarMensaje()` | Object spread `{...datos}` para results + 80+ campo normalization | **MEDIO** — GC pressure |
| **4** | Backend: `registrar_evento` blocking | `await` en loop MQTT para estado/limites | **BAJO** — intermitente, no acumulativo |
| **5** | Frontend: sin batching WS | Cada mensaje se procesa individualmente | **MEDIO** — amplifica #1 y #2 |
| **6** | ESP32: JSON innecesariamente grande | Todos los campos PID enviados siempre | **BAJO** — overhead de parsing |

---

## 4. Estrategias de Solución Propuestas

### Estrategia A: Batching de mensajes en el frontend (IMPACTO ALTO, ESFUERZO BAJO)

> **En palabras simples:**  
> Actualmente, cada vez que llega un dato del péndulo, el navegador lo procesa de inmediato, uno por uno. A 20 mensajes por segundo, el navegador se la pasa trabajando sin parar y no le da tiempo de terminar uno antes de que llegue el siguiente, generando una "fila" que crece con el tiempo.  
> La solución es juntar todos los mensajes que lleguen durante el tiempo entre un cuadro de pantalla y el siguiente (~16ms a 60 FPS), y procesarlos todos de un solo golpe. En vez de procesar 20 veces por segundo, se procesan ~60 veces, pero agrupando los 3-4 mensajes que cayeron en ese intervalo, lo que reduce drásticamente el trabajo repetido.

En `useWebSocket.js`, acumular mensajes y procesarlos en batch vía `requestAnimationFrame`:

```javascript
let _pendingMessages = []
let _rafId = null

ws.onmessage = (evento) => {
  _pendingMessages.push(JSON.parse(evento.data))
  if (!_rafId) {
    _rafId = requestAnimationFrame(() => {
      _rafId = null
      const batch = _pendingMessages
      _pendingMessages = []
      store.procesarLote(batch)  // nuevo método batch
    })
  }
}
```

Limita el procesamiento a **~60 invocaciones/segundo** (tasa del monitor), agrupando los 3-4 mensajes que llegan entre frames.

---

### Estrategia B: `shallowRef` + trigger manual para historial (IMPACTO ALTO, ESFUERZO MEDIO)

> **En palabras simples:**  
> Vue (el framework de la interfaz) funciona observando cada cambio en los datos para saber cuándo redibujar la pantalla. Cada vez que agregamos un nuevo punto a cualquiera de las 40+ series del historial, Vue se entera y ejecuta trabajo interno. A 20 Hz, eso son 800 notificaciones por segundo de "oye, algo cambió".  
> La solución es decirle a Vue: "no vigiles los cambios internos de este objeto". Nosotros le avisamos manualmente cuando queramos que actualice la pantalla, una sola vez por lote, en vez de 800 veces.

Cambiar el historial a `shallowRef` para evitar que Vue rastree cada `.push()`:

```javascript
import { shallowRef, triggerRef } from 'vue'
const historial = shallowRef({ ts: [], angle: [], ... })

function _agregarAlHistorial(datos) {
  const h = historial.value  // sin tracking profundo
  h.ts.push(datos.ts)
  h.angle.push(datos.angle)
  // ... sin overhead de Proxy
}

// Triggear reactividad una sola vez por batch
function _flushHistorial() {
  triggerRef(historial)  // notifica watchers UNA vez
}
```

---

### Estrategia C: Reemplazar `splice()` con Circular Buffer (IMPACTO ALTO, ESFUERZO MEDIO)

> **En palabras simples:**  
> El historial de datos guarda hasta 6000 puntos por cada serie. Cuando se llena, el sistema actual borra los más viejos del inicio de la lista, pero borrar del inicio de una lista larga obliga a mover todo el contenido restante una posición hacia adelante (como quitar al primero de una fila y que todos den un paso). Esto se repite en 40+ listas cada vez que llega un dato.  
> La solución es usar un "buffer circular": en lugar de borrar y mover, simplemente sobreescribimos la posición más vieja con el dato nuevo, como un reloj que vuelve al 12 después del 11. No hay que mover nada.

En lugar de arrays que crecen y se recortan con splice, usar un índice circular:

```javascript
class CircularBuffer {
  constructor(size) {
    this.data = new Float64Array(size)
    this.head = 0
    this.count = 0
    this.size = size
  }
  push(val) {
    this.data[this.head] = val
    this.head = (this.head + 1) % this.size
    if (this.count < this.size) this.count++
  }
  // Para uPlot: retornar vista linealizada
  toArray() { ... }
}
```

Elimina completamente el `splice` y la presión de GC.

---

### Estrategia D: Dropping inteligente en el backend (IMPACTO MEDIO, ESFUERZO BAJO)

> **En palabras simples:**  
> El backend (servidor intermedio) reenvía fielmente todos y cada uno de los mensajes del ESP32 hacia el navegador, sin importar si el navegador puede procesarlos a tiempo. Si el navegador se atrasa, los mensajes se siguen apilando.  
> La solución es que el backend sea más inteligente: si todavía no terminó de enviar el mensaje anterior, descarta los intermedios y solo manda el más reciente. Total, en telemetría lo que importa es ver el estado **actual** del péndulo, no una reproducción perfecta de cada instante pasado.

En `websocket_manager.py`, mantener solo el último mensaje de telemetría pendiente:

```python
class GestorWebSocket:
    def __init__(self):
        self._pending_telemetry = None
        self._broadcast_running = False

    async def broadcast(self, mensaje: dict) -> None:
        if mensaje.get("tipo") == "telemetria":
            self._pending_telemetry = mensaje
            if not self._broadcast_running:
                asyncio.create_task(self._drain_telemetry())
            return
        # Para otros tipos, enviar inmediato
        await self._broadcast_direct(mensaje)

    async def _drain_telemetry(self):
        self._broadcast_running = True
        while self._pending_telemetry:
            msg = self._pending_telemetry
            self._pending_telemetry = None
            await self._broadcast_direct(msg)
        self._broadcast_running = False
```

Garantiza que si el frontend está lento, solo se envía el **último** paquete de telemetría, descartando intermedios.

---

### Estrategia E: Fix `registrar_evento` blocking (IMPACTO BAJO, ESFUERZO TRIVIAL)

> **En palabras simples:**  
> Cuando el ESP32 envía un mensaje de "estado" (no telemetría), el backend se detiene a guardarlo en la base de datos antes de seguir leyendo nuevos mensajes. Si la base de datos tarda (porque está escribiendo en disco), los mensajes de telemetría que llegan mientras tanto se quedan esperando.  
> La solución es que el guardado en base de datos ocurra "en segundo plano", sin detener la lectura de nuevos mensajes.

En `hmi/backend/mqtt_cliente.py`, cambiar `await` a `create_task`:

```python
# Antes:
await self._registro.registrar_evento("estado", datos)

# Después:
asyncio.create_task(self._registro.registrar_evento("estado", datos))
```

---

### Estrategia F: Telemetría modal en el firmware (IMPACTO MEDIO, ESFUERZO MEDIO)

> **En palabras simples:**  
> Actualmente, el ESP32 envía **todos** los datos de **todos** los controladores PID en cada mensaje, aunque solo uno esté activo. Es como si en cada reporte meteorológico te dieran la temperatura de todas las ciudades del mundo cuando solo te interesa la tuya.  
> La solución es que el ESP32 envíe solo los datos del modo que está activo en ese momento. Si estás en PID de posición, solo envía los campos de posición. Esto reduce el tamaño del mensaje de ~800 bytes a ~200-300 bytes, lo que significa menos trabajo para todos: ESP32 (armar el mensaje), red (transmitirlo), y navegador (leerlo y procesarlo).

Enviar solo los campos relevantes al modo activo en vez de todos:

```cpp
void publishTelemetryFast(const TelemetryPacket& p) {
    // Campos comunes siempre
    int len = snprintf(_jsonBuf, sizeof(_jsonBuf),
        "{\"ts\":%lu,\"modo\":\"%s\",\"ol_state\":\"%s\","
        "\"pos_ir_cm\":%.2f,\"pos_mag_cm\":%.2f,\"angle_pend\":%.2f,"
        "\"pwm\":%d,\"direction\":%d", ...);

    // Solo campos del modo activo
    if (p.modo == MODE_PID_POS) {
        len += snprintf(_jsonBuf + len, sizeof(_jsonBuf) - len,
            ",\"pid_error\":%.3f,\"pid_output\":%.1f,...", ...);
    } else if (p.modo == MODE_PID_CASC) { ... }

    _jsonBuf[len++] = '}';
    _jsonBuf[len] = '\0';
    _bridge.publish(TOPIC_TELEMETRY, _jsonBuf);
}
```

Reduce el payload de ~800 bytes a ~200-300 bytes, acelerando serialización en ESP32, transmisión, y parsing en frontend.

---

### Estrategia G: Decimación en el backend — guardar todo, reenviar menos (IMPACTO MEDIO, ESFUERZO BAJO)

> **En palabras simples:**  
> El ESP32 envía 20 mensajes por segundo durante PID activo. El backend los guarda todos en la base de datos (SQLite) para tener un registro histórico completo, pero no necesita reenviar los 20 al navegador: para que las gráficas se vean bien basta con 10 por segundo. Así, el backend actúa como un filtro: almacena el 100% para análisis posterior, pero solo pasa la mitad al frontend, reduciendo su carga a la mitad sin perder resolución en los datos guardados.

En `mqtt_cliente.py`, implementar un contador que reenvíe por WebSocket solo 1 de cada N paquetes de telemetría:

```python
class ClienteMQTT:
    def __init__(self) -> None:
        # ...campos existentes...
        self._telem_counter = 0
        self._telem_ws_divisor = 2  # reenviar 1 de cada 2 (10 Hz de 20 Hz)

    async def _procesar_mensaje(self, msg) -> None:
        # ...parseo existente...
        if topico == config.TOPICO_TELEMETRIA:
            self._ultima_telemetria = datos

            # SIEMPRE guardar en SQLite (resolución completa)
            if self._registro:
                asyncio.create_task(self._registro.registrar_telemetria(datos))

            # Reenviar al frontend solo 1 de cada N
            self._telem_counter += 1
            if self._gestor_ws and (self._telem_counter % self._telem_ws_divisor == 0):
                asyncio.create_task(
                    self._gestor_ws.broadcast({"tipo": "telemetria", "datos": datos})
                )
```

Ventajas:
- **Cero cambios en firmware ni en frontend.**
- El divisor puede ajustarse dinámicamente (p. ej., subir a 4 si se detectan muchos clientes WS lentos).
- Los datos completos quedan respaldados en SQLite para análisis offline a resolución total.

---

## 5. Plan de Implementación Recomendado

| Prioridad | Estrategia | Efecto esperado |
|-----------|-----------|-----------------|
| **1** | A + B: Batching RAF + shallowRef | Elimina ~90% del overhead reactivo |
| **2** | C: Circular buffer | Elimina splice catastrófico |
| **3** | E: Fix await → create_task | Elimina bloqueo intermitente en backend |
| **4** | D: Dropping en backend | Protección contra backpressure |
| **5** | G: Decimación en backend | Reduce carga WS a la mitad sin perder datos en DB |
| **6** | F: Telemetría modal | Reduce payload y overhead general |

Las estrategias **A + B** juntas deberían resolver el problema de acumulación. Las demás son optimizaciones complementarias.

---

## 6. Métricas de Validación

Para confirmar que las optimizaciones funcionan, implementar:

1. **Latencia real en el frontend:** Comparar `ts` del ESP32 con `Date.now()` en el browser (requiere sincronización de reloj, ya existe vía `pendulum/time`).
2. **Mensajes procesados vs recibidos:** Contador de mensajes WS entrantes vs procesados por el store.
3. **Tiempo de procesamiento por batch:** `performance.now()` antes y después de `procesarLote()`.
4. **Queue depth en backend:** Log periódico de tasks pendientes en asyncio.