```zsh
# 1. Mosquitto
cd /Users/admin/Documents/Proyecto_pendulo_invertido
mosquitto -c hmi/mosquitto/mosquitto.conf &

# 2. Backend
cenv/bin/uvicorn hmi.backend.app:app --host 0.0.0.0 --port 8000 &

# 3. Frontend
cd hmi/frontend && npm run dev &

# 4. ESP32 (solo si necesitas re-flashear)
cd /Users/admin/Documents/Proyecto_pendulo_invertido/SPISC
~/.platformio/penv/bin/pio run -e esp32dev -t upload
```



