"""
Aplicación FastAPI principal — HMI Péndulo Invertido.
REST API + WebSocket + MQTT bridge.
"""

from __future__ import annotations

import logging
import time
from contextlib import asynccontextmanager

import orjson
from fastapi import FastAPI, WebSocket, Query
from fastapi.middleware.cors import CORSMiddleware

from . import config
from .modelos import (
    ComandoMotor,
    ComandoModo,
    ComandoCalibracion,
    ComandoConfig,
    ComandoStop,
    ComandoSweep,
)
from .mqtt_cliente import ClienteMQTT
from .websocket_manager import GestorWebSocket
from .registro_datos import RegistroDatos

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
)
logger = logging.getLogger("hmi.app")

# ── Instancias globales ──────────────────────────
mqtt = ClienteMQTT()
gestor_ws = GestorWebSocket()
registro = RegistroDatos()

# Vincular dependencias cruzadas
mqtt.vincular(gestor_ws, registro)
gestor_ws.vincular_mqtt(mqtt)


# ── Ciclo de vida de la app ──────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    await registro.iniciar()
    await mqtt.iniciar()
    logger.info("HMI Backend iniciado — MQTT %s:%d", config.MQTT_HOST, config.MQTT_PORT)
    yield
    await mqtt.detener()
    await registro.detener()
    logger.info("HMI Backend detenido")


app = FastAPI(
    title="HMI Péndulo Invertido",
    description="Backend para la interfaz de supervisión y control del péndulo invertido",
    version="0.1.0",
    lifespan=lifespan,
)

# ── CORS ─────────────────────────────────────────
app.add_middleware(
    CORSMiddleware,
    allow_origins=config.CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# ══════════════════════════════════════════════════
#  WebSocket
# ══════════════════════════════════════════════════

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await gestor_ws.conectar(ws)
    await gestor_ws.escuchar(ws)


# ══════════════════════════════════════════════════
#  REST — Lectura de estado
# ══════════════════════════════════════════════════

@app.get("/api/estado")
async def obtener_estado():
    """Estado actual del sistema (último paquete recibido)."""
    return {
        "estado": mqtt.estado_actual,
        "telemetria": mqtt.telemetria_actual,
        "limites": mqtt.limites_actual,
        "mqtt_conectado": mqtt.conectado,
        "ws_clientes": gestor_ws.num_clientes,
    }


@app.get("/api/historial")
async def obtener_historial(
    desde: float = Query(default=0, description="Timestamp UNIX inicio"),
    hasta: float = Query(default=0, description="Timestamp UNIX fin (0 = ahora)"),
    limite: int = Query(default=5000, ge=1, le=50000),
):
    """Telemetría histórica guardada en SQLite."""
    if hasta <= 0:
        hasta = time.time()
    if desde <= 0:
        desde = hasta - 60  # Último minuto por defecto
    datos = await registro.consultar_historial(desde, hasta, limite)
    return {"registros": len(datos), "datos": datos}


@app.get("/api/sesiones")
async def obtener_sesiones(
    limite: int = Query(default=50, ge=1, le=500),
):
    """Lista de sesiones de datos registradas."""
    sesiones = await registro.consultar_sesiones(limite)
    return {"sesiones": sesiones}


# ══════════════════════════════════════════════════
#  REST — Envío de comandos
# ══════════════════════════════════════════════════

@app.post("/api/comando/motor")
async def comando_motor(cmd: ComandoMotor):
    ok = await mqtt.publicar(config.TOPICO_CMD_MOTOR, cmd.model_dump())
    return {"enviado": ok}


@app.post("/api/comando/modo")
async def comando_modo(cmd: ComandoModo):
    ok = await mqtt.publicar(config.TOPICO_CMD_MODO, cmd.model_dump())
    return {"enviado": ok}


@app.post("/api/comando/calibrar")
async def comando_calibrar(cmd: ComandoCalibracion):
    ok = await mqtt.publicar(config.TOPICO_CMD_CALIBRAR, cmd.model_dump())
    return {"enviado": ok}


@app.post("/api/comando/config")
async def comando_config(cmd: ComandoConfig):
    ok = await mqtt.publicar(config.TOPICO_CMD_CONFIG, cmd.model_dump())
    return {"enviado": ok}


@app.post("/api/comando/stop")
async def comando_stop():
    ok = await mqtt.publicar(config.TOPICO_CMD_STOP, {})
    return {"enviado": ok}


@app.post("/api/comando/sweep")
async def comando_sweep(cmd: ComandoSweep):
    ok = await mqtt.publicar(config.TOPICO_CMD_SWEEP, cmd.model_dump())
    return {"enviado": ok}
