"""
Cliente MQTT asíncrono — Puente entre el broker Mosquitto y el backend FastAPI.
Se suscribe a los tópicos de datos del ESP32 (o mock) y publica comandos.
"""

from __future__ import annotations

import asyncio
import logging
from typing import TYPE_CHECKING

import aiomqtt
import orjson

from . import config

if TYPE_CHECKING:
    from .websocket_manager import GestorWebSocket
    from .registro_datos import RegistroDatos

logger = logging.getLogger("hmi.mqtt")


class ClienteMQTT:
    """Gestiona la conexión MQTT con reconexión automática."""

    def __init__(self) -> None:
        self._gestor_ws: GestorWebSocket | None = None
        self._registro: RegistroDatos | None = None
        self._client: aiomqtt.Client | None = None
        self._ultimo_estado: dict | None = None
        self._ultima_telemetria: dict | None = None
        self._ultimos_limites: dict | None = None
        self._tarea: asyncio.Task | None = None

    # ── Inyección de dependencias ─────────────────
    def vincular(
        self,
        gestor_ws: GestorWebSocket,
        registro: RegistroDatos,
    ) -> None:
        self._gestor_ws = gestor_ws
        self._registro = registro

    # ── Ciclo de vida ─────────────────────────────
    async def iniciar(self) -> None:
        """Lanza el loop de escucha MQTT en background."""
        self._tarea = asyncio.create_task(self._loop_conexion())

    async def detener(self) -> None:
        if self._tarea:
            self._tarea.cancel()
            try:
                await self._tarea
            except asyncio.CancelledError:
                pass
        logger.info("Cliente MQTT detenido")

    # ── Loop principal con reconexión ─────────────
    async def _loop_conexion(self) -> None:
        espera = 1
        while True:
            try:
                async with aiomqtt.Client(
                    hostname=config.MQTT_HOST,
                    port=config.MQTT_PORT,
                    identifier="hmi-backend",
                ) as client:
                    self._client = client
                    espera = 1  # reset backoff
                    logger.info(
                        "Conectado a MQTT %s:%d", config.MQTT_HOST, config.MQTT_PORT
                    )

                    await client.subscribe(config.TOPICO_SUB_TODOS)

                    async for msg in client.messages:
                        try:
                            await self._procesar_mensaje(msg)
                        except Exception:
                            logger.exception("Error procesando mensaje MQTT %s", msg.topic)

            except aiomqtt.MqttError as e:
                logger.warning("Conexión MQTT perdida: %s — reintentando en %ds", e, espera)
            except asyncio.CancelledError:
                break
            except Exception:
                logger.exception("Error inesperado en loop MQTT — reintentando en %ds", espera)
            finally:
                self._client = None
            await asyncio.sleep(espera)
            espera = min(espera * 2, 30)

    # ── Procesamiento de mensajes entrantes ───────
    async def _procesar_mensaje(self, msg: aiomqtt.Message) -> None:
        topico = str(msg.topic)
        try:
            datos = orjson.loads(msg.payload)
        except (orjson.JSONDecodeError, TypeError):
            logger.debug("Payload no-JSON en %s: %s", topico, msg.payload)
            return

        if topico == config.TOPICO_TELEMETRIA:
            self._ultima_telemetria = datos
            if self._gestor_ws:
                await self._gestor_ws.broadcast({"tipo": "telemetria", "datos": datos})
            if self._registro:
                await self._registro.registrar_telemetria(datos)

        elif topico == config.TOPICO_ESTADO:
            self._ultimo_estado = datos
            if self._gestor_ws:
                await self._gestor_ws.broadcast({"tipo": "estado", "datos": datos})
            if self._registro:
                await self._registro.registrar_evento("estado", datos)

        elif topico == config.TOPICO_LIMITES:
            self._ultimos_limites = datos
            if self._gestor_ws:
                await self._gestor_ws.broadcast({"tipo": "limites", "datos": datos})
            if self._registro:
                await self._registro.registrar_evento("limites", datos)

        elif topico == config.TOPICO_SWEEP_RESULTS:
            if self._gestor_ws:
                await self._gestor_ws.broadcast({"tipo": "sweep_results", "datos": datos})
            if self._registro:
                await self._registro.registrar_evento("sweep_results", datos)

    # ── Publicación de comandos ───────────────────
    async def publicar(self, topico: str, payload: dict) -> bool:
        """Publica un JSON en un tópico MQTT. Retorna True si se envió."""
        if not self._client:
            logger.warning("No conectado a MQTT — comando descartado")
            return False
        try:
            await self._client.publish(topico, orjson.dumps(payload))
            logger.info("Publicado en %s: %s", topico, payload)
            return True
        except aiomqtt.MqttError as e:
            logger.error("Error publicando en %s: %s", topico, e)
            return False

    # ── Snapshots (para endpoints REST) ───────────
    @property
    def estado_actual(self) -> dict | None:
        return self._ultimo_estado

    @property
    def telemetria_actual(self) -> dict | None:
        return self._ultima_telemetria

    @property
    def limites_actual(self) -> dict | None:
        return self._ultimos_limites

    @property
    def conectado(self) -> bool:
        return self._client is not None
