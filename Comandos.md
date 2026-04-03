
# 1. Mosquitto
```zsh
cd /Users/admin/Documents/Proyectos/pendulo_invertido

# Para matar procesos de mosquitto
killall mosquitto 2>/dev/null

# Para activarlo (con logs verbose)
mosquitto -c hmi/mosquitto/mosquitto.conf -v &

```

# 2. Backend
```zsh
cenv/bin/uvicorn hmi.backend.app:app --host 0.0.0.0 --port 8000 &
``` 
# 3. Frontend
```zsh
cd hmi/frontend && npm run dev &
``` 

# 4. ESP32 (compilar y re-flashear)
```zsh
cd /Users/admin/Documents/Proyectos/pendulo_invertido/SPISC

// Compilar
~/.platformio/penv/bin/pio run -e esp32dev

// Flashear (subir a la ESP32)
~/.platformio/penv/bin/pio run -e esp32dev -t upload
``` 

# 5. Monitor ESP32
```zsh
~/.platformio/penv/bin/pio device monitor -b 115200
```

# 6. Verificar la IP del broker (PC donde corre Mosquitto)
```zsh
ipconfig getifaddr en0
```
