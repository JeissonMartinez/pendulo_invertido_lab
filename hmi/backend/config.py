"""
Configuración centralizada del backend HMI — Péndulo Invertido.
Valores por defecto para desarrollo local; sobreescribibles con variables de entorno.
"""

import os
from pathlib import Path

# ── Rutas ─────────────────────────────────────────
BASE_DIR = Path(__file__).resolve().parent
DB_PATH = BASE_DIR / "pendulo.db"

# ── MQTT Broker ───────────────────────────────────
MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))

# ── Tópicos MQTT ──────────────────────────────────
TOPICO_TELEMETRIA = "pendulum/telemetry"
TOPICO_ESTADO = "pendulum/status"
TOPICO_LIMITES = "pendulum/limits"
TOPICO_CMD_MOTOR = "pendulum/cmd/motor"
TOPICO_CMD_MODO = "pendulum/cmd/mode"
TOPICO_CMD_CALIBRAR = "pendulum/cmd/calibrate"
TOPICO_CMD_CONFIG = "pendulum/cmd/config"
TOPICO_CMD_STOP = "pendulum/cmd/stop"
TOPICO_CMD_SWEEP = "pendulum/cmd/sweep"
TOPICO_CMD_OPENLOOP = "pendulum/cmd/openloop"

# Resultados de barrido PWM
TOPICO_SWEEP_RESULTS = "pendulum/sweep/results"

# Patrón para suscripción a todos los tópicos del péndulo
TOPICO_SUB_TODOS = "pendulum/#"
# Tópicos de datos (ESP32 → Backend)
TOPICOS_DATOS = {TOPICO_TELEMETRIA, TOPICO_ESTADO, TOPICO_LIMITES, TOPICO_SWEEP_RESULTS}

# ── Backend ───────────────────────────────────────
BACKEND_HOST = os.getenv("BACKEND_HOST", "0.0.0.0")
BACKEND_PORT = int(os.getenv("BACKEND_PORT", "8000"))

# ── Frontend (CORS) ──────────────────────────────
CORS_ORIGINS = [
    "http://localhost:5173",   # Vite dev server
    "http://127.0.0.1:5173",
    "http://localhost:4173",   # Vite preview
]

# ── Registro de datos ─────────────────────────────
REGISTRO_BATCH_SIZE = 50           # Registros por lote de inserción
REGISTRO_FLUSH_INTERVAL_S = 2.0   # Forzar flush cada N segundos
