```zsh
# 1. Mosquitto
cd /Users/admin/Documents/Proyectos/pendulo_invertido

# Para matar procesos de mosquitto
killall mosquitto 2>/dev/null

# Para activarlo
mosquitto -c hmi/mosquitto/mosquitto.conf &

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



