"""
Gestor de conexiones WebSocket — Broadcast de telemetría y recepción de comandos.
"""

from __future__ import annotations

import asyncio
import logging
from typing import TYPE_CHECKING

import orjson
from fastapi import WebSocket, WebSocketDisconnect

if TYPE_CHECKING:
    from .mqtt_cliente import ClienteMQTT

from . import config

logger = logging.getLogger("hmi.ws")


class GestorWebSocket:
    """Administra las conexiones WebSocket de clientes frontend."""

    def __init__(self) -> None:
        self._conexiones: set[WebSocket] = set()
        self._mqtt: ClienteMQTT | None = None

    def vincular_mqtt(self, mqtt: ClienteMQTT) -> None:
        self._mqtt = mqtt

    # ── Gestión de conexiones ─────────────────────
    async def conectar(self, ws: WebSocket) -> None:
        await ws.accept()
        self._conexiones.add(ws)
        logger.info("WebSocket conectado — %d clientes activos", len(self._conexiones))

    def desconectar(self, ws: WebSocket) -> None:
        self._conexiones.discard(ws)
        logger.info("WebSocket desconectado — %d clientes activos", len(self._conexiones))

    # ── Broadcast a todos los clientes ────────────
    async def broadcast(self, mensaje: dict) -> None:
        """Envía un mensaje JSON a todas las conexiones activas."""
        if not self._conexiones:
            return

        datos_text = orjson.dumps(mensaje).decode("utf-8")
        desconectados: list[WebSocket] = []

        for ws in self._conexiones:
            try:
                await ws.send_text(datos_text)
            except Exception:
                desconectados.append(ws)

        for ws in desconectados:
            self._conexiones.discard(ws)

    # ── Escucha de un cliente (comandos entrantes) ─
    async def escuchar(self, ws: WebSocket) -> None:
        """Loop de recepción de mensajes de un cliente WebSocket."""
        try:
            while True:
                datos_raw = await ws.receive_text()
                try:
                    mensaje = orjson.loads(datos_raw)
                except (orjson.JSONDecodeError, TypeError):
                    continue

                try:
                    await self._procesar_comando(mensaje)
                except Exception:
                    logger.exception("Error procesando comando WS: %s", mensaje.get('tipo', '?'))
        except WebSocketDisconnect:
            pass
        finally:
            self.desconectar(ws)

    # ── Procesamiento de comandos del frontend ────
    async def _procesar_comando(self, mensaje: dict) -> None:
        """Recibe un comando del frontend y lo publica en MQTT."""
        if not self._mqtt:
            return

        tipo = mensaje.get("tipo", "")
        datos = mensaje.get("datos", {})

        mapa_topicos = {
            "cmd_motor": config.TOPICO_CMD_MOTOR,
            "cmd_modo": config.TOPICO_CMD_MODO,
            "cmd_calibrar": config.TOPICO_CMD_CALIBRAR,
            "cmd_config": config.TOPICO_CMD_CONFIG,
            "cmd_stop": config.TOPICO_CMD_STOP,
            "cmd_sweep": config.TOPICO_CMD_SWEEP,
            "cmd_openloop": config.TOPICO_CMD_OPENLOOP,
        }

        topico = mapa_topicos.get(tipo)
        if topico:
            await self._mqtt.publicar(topico, datos)
        else:
            logger.warning("Tipo de comando desconocido: %s", tipo)

    @property
    def num_clientes(self) -> int:
        return len(self._conexiones)
