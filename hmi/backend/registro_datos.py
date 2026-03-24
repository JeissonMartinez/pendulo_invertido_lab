"""
Registro de datos — Persistencia de telemetría y eventos en SQLite async.
Escritura por lotes para rendimiento con datos a ~50 Hz.
"""

from __future__ import annotations

import asyncio
import logging
import time

import aiosqlite
import orjson

from . import config

logger = logging.getLogger("hmi.registro")

SQL_CREAR_TABLAS = """
CREATE TABLE IF NOT EXISTS telemetria (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_servidor REAL    NOT NULL,
    ts_esp32    INTEGER,
    angle       REAL,
    position    REAL,
    cart_accel  REAL,
    rpm         REAL,
    pwm         INTEGER,
    direction   TEXT,
    -- Campos extendidos (sensores completos)
    pulsos_ir   INTEGER,
    vueltas_ir  REAL,
    rps_ir      REAL,
    rpm_ir      REAL,
    pos_ir_cm   REAL,
    rps_mag     REAL,
    rpm_mag     REAL,
    pos_mag_cm  REAL,
    angle_pend  REAL,
    vel_ang_pend REAL,
    mpu_ax      REAL,
    mpu_ay      REAL,
    mpu_az      REAL,
    mpu_gx      REAL,
    mpu_gy      REAL,
    mpu_gz      REAL,
    mpu_pitch   REAL,
    mpu_roll    REAL,
    ls_start    INTEGER,
    ls_end      INTEGER,
    modo        TEXT
);

CREATE TABLE IF NOT EXISTS eventos (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_servidor REAL    NOT NULL,
    tipo        TEXT    NOT NULL,
    detalle     TEXT
);

CREATE TABLE IF NOT EXISTS sesiones (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    inicio      REAL    NOT NULL,
    fin         REAL,
    modo        TEXT,
    notas       TEXT
);

CREATE INDEX IF NOT EXISTS idx_telemetria_ts ON telemetria(ts_servidor);
CREATE INDEX IF NOT EXISTS idx_eventos_ts ON eventos(ts_servidor);
"""


class RegistroDatos:
    """Logger asíncrono con buffer batch para SQLite."""

    def __init__(self) -> None:
        self._db: aiosqlite.Connection | None = None
        self._buffer_telemetria: list[tuple] = []
        self._buffer_eventos: list[tuple] = []
        self._tarea_flush: asyncio.Task | None = None
        self._sesion_id: int | None = None

    # ── Ciclo de vida ─────────────────────────────
    async def iniciar(self) -> None:
        self._db = await aiosqlite.connect(str(config.DB_PATH))
        await self._db.executescript(SQL_CREAR_TABLAS)
        await self._db.commit()
        self._tarea_flush = asyncio.create_task(self._loop_flush())
        logger.info("Registro de datos iniciado — DB: %s", config.DB_PATH)

    async def detener(self) -> None:
        if self._tarea_flush:
            self._tarea_flush.cancel()
            try:
                await self._tarea_flush
            except asyncio.CancelledError:
                pass
        await self._flush()
        if self._db:
            await self._db.close()
        logger.info("Registro de datos detenido")

    # ── Inserción en buffer ───────────────────────
    async def registrar_telemetria(self, datos: dict) -> None:
        self._buffer_telemetria.append((
            time.time(),
            datos.get("ts"),
            datos.get("angle"),
            datos.get("position"),
            datos.get("cart_accel"),
            datos.get("rpm"),
            datos.get("pwm"),
            datos.get("direction"),
            datos.get("pulsos_ir"),
            datos.get("vueltas_ir"),
            datos.get("rps_ir"),
            datos.get("rpm_ir"),
            datos.get("pos_ir_cm"),
            datos.get("rps_mag"),
            datos.get("rpm_mag"),
            datos.get("pos_mag_cm"),
            datos.get("angle_pend"),
            datos.get("vel_ang_pend"),
            datos.get("mpu_ax"),
            datos.get("mpu_ay"),
            datos.get("mpu_az"),
            datos.get("mpu_gx"),
            datos.get("mpu_gy"),
            datos.get("mpu_gz"),
            datos.get("mpu_pitch"),
            datos.get("mpu_roll"),
            1 if datos.get("ls_start") else 0,
            1 if datos.get("ls_end") else 0,
            datos.get("modo"),
        ))
        if len(self._buffer_telemetria) >= config.REGISTRO_BATCH_SIZE:
            await self._flush_telemetria()

    async def registrar_evento(self, tipo: str, detalle: dict) -> None:
        self._buffer_eventos.append((
            time.time(),
            tipo,
            orjson.dumps(detalle).decode(),
        ))
        if len(self._buffer_eventos) >= config.REGISTRO_BATCH_SIZE:
            await self._flush_eventos()

    # ── Flush periódico ───────────────────────────
    async def _loop_flush(self) -> None:
        while True:
            await asyncio.sleep(config.REGISTRO_FLUSH_INTERVAL_S)
            await self._flush()

    async def _flush(self) -> None:
        await self._flush_telemetria()
        await self._flush_eventos()

    async def _flush_telemetria(self) -> None:
        if not self._buffer_telemetria or not self._db:
            return
        lote = self._buffer_telemetria[:]
        self._buffer_telemetria.clear()
        await self._db.executemany(
            "INSERT INTO telemetria (ts_servidor, ts_esp32, angle, position, "
            "cart_accel, rpm, pwm, direction, "
            "pulsos_ir, vueltas_ir, rps_ir, rpm_ir, pos_ir_cm, "
            "rps_mag, rpm_mag, pos_mag_cm, "
            "angle_pend, vel_ang_pend, "
            "mpu_ax, mpu_ay, mpu_az, mpu_gx, mpu_gy, mpu_gz, mpu_pitch, mpu_roll, "
            "ls_start, ls_end, modo) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            lote,
        )
        await self._db.commit()

    async def _flush_eventos(self) -> None:
        if not self._buffer_eventos or not self._db:
            return
        lote = self._buffer_eventos[:]
        self._buffer_eventos.clear()
        await self._db.executemany(
            "INSERT INTO eventos (ts_servidor, tipo, detalle) VALUES (?, ?, ?)",
            lote,
        )
        await self._db.commit()

    # ── Sesiones ──────────────────────────────────
    async def iniciar_sesion(self, modo: str = "", notas: str = "") -> int:
        if not self._db:
            return -1
        cursor = await self._db.execute(
            "INSERT INTO sesiones (inicio, modo, notas) VALUES (?, ?, ?)",
            (time.time(), modo, notas),
        )
        await self._db.commit()
        self._sesion_id = cursor.lastrowid
        return self._sesion_id  # type: ignore[return-value]

    async def finalizar_sesion(self) -> None:
        if not self._db or not self._sesion_id:
            return
        await self._db.execute(
            "UPDATE sesiones SET fin = ? WHERE id = ?",
            (time.time(), self._sesion_id),
        )
        await self._db.commit()
        self._sesion_id = None

    # ── Consultas ─────────────────────────────────
    async def consultar_historial(
        self, desde: float, hasta: float, limite: int = 5000
    ) -> list[dict]:
        if not self._db:
            return []
        cursor = await self._db.execute(
            "SELECT ts_servidor, ts_esp32, angle, position, cart_accel, rpm, pwm, direction, "
            "pulsos_ir, vueltas_ir, rps_ir, rpm_ir, pos_ir_cm, "
            "rps_mag, rpm_mag, pos_mag_cm, "
            "angle_pend, vel_ang_pend, "
            "mpu_ax, mpu_ay, mpu_az, mpu_gx, mpu_gy, mpu_gz, mpu_pitch, mpu_roll, "
            "ls_start, ls_end, modo "
            "FROM telemetria WHERE ts_servidor BETWEEN ? AND ? "
            "ORDER BY ts_servidor LIMIT ?",
            (desde, hasta, limite),
        )
        columnas = [
            "ts_servidor", "ts_esp32", "angle", "position", "cart_accel", "rpm", "pwm", "direction",
            "pulsos_ir", "vueltas_ir", "rps_ir", "rpm_ir", "pos_ir_cm",
            "rps_mag", "rpm_mag", "pos_mag_cm",
            "angle_pend", "vel_ang_pend",
            "mpu_ax", "mpu_ay", "mpu_az", "mpu_gx", "mpu_gy", "mpu_gz", "mpu_pitch", "mpu_roll",
            "ls_start", "ls_end", "modo",
        ]
        return [dict(zip(columnas, fila)) for fila in await cursor.fetchall()]

    async def consultar_sesiones(self, limite: int = 50) -> list[dict]:
        if not self._db:
            return []
        cursor = await self._db.execute(
            "SELECT id, inicio, fin, modo, notas FROM sesiones ORDER BY inicio DESC LIMIT ?",
            (limite,),
        )
        columnas = ["id", "inicio", "fin", "modo", "notas"]
        return [dict(zip(columnas, fila)) for fila in await cursor.fetchall()]
