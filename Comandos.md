```zsh
# 1. Mosquitto
cd /Users/admin/Documents/Proyectos/pendulo_invertido

# Para matar procesos de mosquitto
killall mosquitto 2>/dev/null

# Para activarlo (con logs verbose)
mosquitto -c hmi/mosquitto/mosquitto.conf -v &

# 2. Backend
cenv/bin/uvicorn hmi.backend.app:app --host 0.0.0.0 --port 8000 &

# 3. Frontend
cd hmi/frontend && npm run dev &

# 4. ESP32 (solo si necesitas re-flashear)
cd /Users/admin/Documents/Proyectos/pendulo_invertido/SPISC
~/.platformio/penv/bin/pio run -e esp32dev -t upload

# 5. Monitor ESP32
~/.platformio/penv/bin/pio device monitor -b 115200

```


Probar cambios en Dual_core:

```zsh
Para validar la implementación dual-core:

## 1. Flashear el firmware

```bash
cd /Users/admin/Documents/Proyectos/pendulo_invertido/SPISC
~/.platformio/penv/bin/pio run -t upload -e esp32dev
```

## 2. Monitorear salida serial

```bash
~/.platformio/penv/bin/pio device monitor -b 115200
```

**Salida esperada al arrancar:**
```
[main] ═══ SPISC — Firmware Dual-Core ═══
[main] Motor OK
[main] Encoder IR OK
[main] Limit switches OK
[main] Sensores I2C OK
[main] Mutex y cola OK
[MqttComm] Cola de comandos inicializada
[main] WiFi + MQTT OK
[main] Safety OK
[Core0] Tarea de sensores iniciada
[safety] Watchdog hardware iniciado (5 s)
[Core1] Tarea de comunicación iniciada
[main] ═══ Sistema dual-core activo ═══
```

## 3. Configurar broker MQTT

Según tu configuración (`CFG_WIFI_AP` activo), el ESP32 crea red WiFi:

1. Conectar tu PC a WiFi **ESP32-Pendulum** (pass: `pendulo123`)
2. Iniciar Mosquitto en tu PC:
   ```bash
   cd /Users/admin/Documents/Proyectos/pendulo_invertido/hmi/mosquitto
   mosquitto -c mosquitto.conf -v
   ```

## 4. Probar comunicación MQTT

En otra terminal, suscribirse a telemetría:
```bash
mosquitto_sub -h 192.168.4.1 -t "pendulum/#" -v
```

Enviar comando de prueba:
```bash
mosquitto_pub -h 192.168.4.1 -t "pendulum/cmd/mode" -m '{"mode":"IDLE"}'
```

## 5. Verificar métricas clave

| Verificación | Cómo comprobarlo |
|--------------|------------------|
| Sensores @ 500Hz | Telemetría llega cada 200ms con `ts` incrementando ≈200 |
| Sin bloqueos por OLED | El timestamp no salta >250ms entre paquetes |
| Comandos procesados | Enviar `cmd/stop` → ver motor para en serial |
| Watchdog activo | Si bloqueas Core 0, ESP32 resetea en 5s |

## 6. Test de stress (opcional)

Enviar comandos rápidos mientras observas que la telemetría sigue llegando estable:
```bash
for i in {1..10}; do
  mosquitto_pub -h 192.168.4.1 -t "pendulum/cmd/calibrate" -m '{"type":"angle"}'
  sleep 0.1
done
```

¿Tienes el hardware conectado para hacer la prueba ahora?
```


