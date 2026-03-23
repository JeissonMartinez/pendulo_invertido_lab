#!/usr/bin/env python3
"""
Simulador Mock del Péndulo Invertido — Reemplaza al ESP32 para desarrollo de la HMI.

Publica telemetría MQTT idéntica a la que generaría el firmware real.
Se suscribe a comandos y ajusta la simulación en consecuencia.

Uso:
    python -m hmi.mock.simulador_pendulo          # desde raíz del proyecto
    python hmi/mock/simulador_pendulo.py           # directo

Requiere: aiomqtt, orjson  (pip install aiomqtt orjson)
Requiere: Mosquitto corriendo en localhost:1883
"""

from __future__ import annotations

import asyncio
import logging
import math
import os
import random
import sys
import time

import aiomqtt
import orjson

# ── Configuración ─────────────────────────────────
MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))

FREQ_TELEMETRIA = 50   # Hz
FREQ_ESTADO = 2        # Hz
DT = 1.0 / FREQ_TELEMETRIA

# Tópicos (idénticos al backend)
T_TELEMETRIA = "pendulum/telemetry"
T_ESTADO = "pendulum/status"
T_LIMITES = "pendulum/limits"
T_CMD = "pendulum/cmd/#"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [MOCK] %(levelname)s: %(message)s",
)
logger = logging.getLogger("mock")


# ══════════════════════════════════════════════════
#  Modelo físico simplificado del péndulo invertido
# ══════════════════════════════════════════════════

class ModeloPendulo:
    """
    Simulación simplificada basada en las ecuaciones linealizadas
    del péndulo invertido sobre carro.

    Variables de estado:
        theta   — ángulo del péndulo (rad), 0 = abajo, π = arriba
        theta_d — velocidad angular (rad/s)
        x       — posición del carro (cm), 0..95
        x_d     — velocidad del carro (cm/s)

    La salida se convierte a las unidades del firmware:
        angle    — grados (0–360), 180 = vertical arriba
        position — cm (0–95)
    """

    # Parámetros físicos aproximados
    G = 9.81          # gravedad (m/s²)
    L = 0.30          # longitud barra (m)
    M_CARRO = 0.5     # masa carro (kg)
    M_PENDULO = 0.15  # masa péndulo (kg)
    FRICCION = 0.1    # amortiguamiento
    PISTA_CM = 95.0   # longitud pista

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        """Estado IDLE: péndulo colgando abajo, carro centrado."""
        self.theta = 0.01       # casi 0 (abajo) — pequeña perturbación
        self.theta_d = 0.0
        self.x = 45.0           # centro de la pista (cm)
        self.x_d = 0.0
        self.pwm = 0
        self.direction = "DETENIDO"
        self.rpm = 0.0
        self.cart_accel = 0.0

    def step(self, dt: float, fuerza_norm: float) -> None:
        """
        Avanza la simulación un paso.
        fuerza_norm: -1.0 .. +1.0 (proporcional a PWM/1023)
        """
        g = self.G
        L = self.L
        mc = self.M_CARRO
        mp = self.M_PENDULO
        b = self.FRICCION

        # Fuerza aplicada al carro (N) — escala arbitraria
        F = fuerza_norm * 15.0

        sin_t = math.sin(self.theta)
        cos_t = math.cos(self.theta)
        total_m = mc + mp

        # Ecuaciones de movimiento (péndulo no lineal sobre carro)
        denom = L * (total_m - mp * cos_t * cos_t)
        if abs(denom) < 1e-9:
            denom = 1e-9

        theta_dd = (
            g * total_m * sin_t
            - cos_t * (F + mp * L * self.theta_d**2 * sin_t)
            - b * self.theta_d
        ) / denom

        x_dd_m = (
            F + mp * L * (self.theta_d**2 * sin_t - theta_dd * cos_t)
        ) / total_m  # m/s²

        # Integración Euler semi-implícita
        self.theta_d += theta_dd * dt
        self.theta += self.theta_d * dt

        x_dd_cm = x_dd_m * 100.0  # convertir a cm/s²
        self.x_d += x_dd_cm * dt
        self.x += self.x_d * dt

        # Limitar posición a la pista
        if self.x <= 0:
            self.x = 0
            self.x_d = max(0, self.x_d)
        elif self.x >= self.PISTA_CM:
            self.x = self.PISTA_CM
            self.x_d = min(0, self.x_d)

        # Normalizar theta a [0, 2π)
        self.theta = self.theta % (2 * math.pi)

        # Actualizar variables de salida
        self.cart_accel = x_dd_m  # m/s² para el reporte
        self.rpm = abs(self.x_d) * 2.0  # estimación simple RPM

    @property
    def angle_deg(self) -> float:
        """Ángulo en grados (0 = abajo, 180 = arriba), como el firmware."""
        return math.degrees(self.theta)

    @property
    def position_cm(self) -> float:
        return self.x

    @property
    def limit_start(self) -> bool:
        return self.x <= 0.5

    @property
    def limit_end(self) -> bool:
        return self.x >= self.PISTA_CM - 0.5


# ══════════════════════════════════════════════════
#  Controlador PID simple (para modo CONTROLLING)
# ══════════════════════════════════════════════════

class PIDSimple:
    def __init__(self, kp: float, ki: float, kd: float) -> None:
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.integral = 0.0
        self.error_prev = 0.0

    def reset(self) -> None:
        self.integral = 0.0
        self.error_prev = 0.0

    def compute(self, error: float, dt: float) -> float:
        self.integral += error * dt
        self.integral = max(-50, min(50, self.integral))  # anti-windup
        derivada = (error - self.error_prev) / dt if dt > 0 else 0
        self.error_prev = error
        return self.kp * error + self.ki * self.integral + self.kd * derivada


# ══════════════════════════════════════════════════
#  Simulador principal
# ══════════════════════════════════════════════════

class SimuladorPendulo:
    """
    Máquina de estados del mock, publicación MQTT, y respuesta a comandos.
    """

    ESTADOS_VALIDOS = {"IDLE", "CALIBRATING", "SWING_UP", "CONTROLLING", "STOPPING", "EMERGENCY"}

    def __init__(self) -> None:
        self.modelo = ModeloPendulo()
        self.estado = "IDLE"
        self.modo = "MANUAL"
        self.errores: list[str] = []
        self.calibrado_angulo = False
        self.calibrado_posicion = False

        # Control PID para modo CONTROLLING
        self.pid_angulo = PIDSimple(kp=30.0, ki=0.5, kd=5.0)
        self.pid_posicion = PIDSimple(kp=0.5, ki=0.0, kd=0.2)

        # Para calibración simulada
        self._calibracion_inicio: float = 0
        self._calibracion_tipo: str = ""
        self._calibracion_progreso: float = 0

        # Para swing-up simulado
        self._swingup_t: float = 0

        # Comando manual
        self._cmd_pwm: int = 0
        self._cmd_dir: str = "DETENIDO"

        # Estado anterior de límites (para on-change)
        self._prev_limits = (False, False)

        # Millis simulado
        self._t0 = time.time()

    @property
    def millis(self) -> int:
        return int((time.time() - self._t0) * 1000)

    # ── Loop principal de simulación ──────────────
    def tick(self) -> None:
        """Avanza un paso de simulación según el estado actual."""
        if self.estado == "IDLE":
            # Péndulo oscila libremente con gravedad, sin fuerza
            self.modelo.step(DT, 0.0)

        elif self.estado == "CALIBRATING":
            self._tick_calibrating()

        elif self.estado == "SWING_UP":
            self._tick_swingup()

        elif self.estado == "CONTROLLING":
            self._tick_controlling()

        elif self.estado == "STOPPING":
            # Frenar gradualmente
            self.modelo.x_d *= 0.9
            self.modelo.step(DT, 0.0)
            if abs(self.modelo.x_d) < 0.5:
                self.estado = "IDLE"

        elif self.estado == "EMERGENCY":
            self.modelo.pwm = 0
            self.modelo.direction = "DETENIDO"
            self.modelo.x_d *= 0.8
            self.modelo.step(DT, 0.0)
            if abs(self.modelo.x_d) < 0.1:
                self.estado = "IDLE"

        # Agregar ruido de sensor
        self._agregar_ruido()

        # Actualizar PWM y dirección de salida
        self._actualizar_salida()

    def _tick_calibrating(self) -> None:
        elapsed = time.time() - self._calibracion_inicio
        duracion = 6.0 if self._calibracion_tipo == "full" else 3.0
        self._calibracion_progreso = min(1.0, elapsed / duracion)

        if self._calibracion_tipo in ("angle", "full"):
            # Simular: ángulo se estabiliza suavemente hacia medición estable
            self.modelo.theta_d *= 0.95

        if self._calibracion_tipo in ("position", "full"):
            # Simular: carro se mueve lento de un extremo a otro
            fase = elapsed / duracion
            if fase < 0.4:
                self.modelo.step(DT, -0.15)  # ir a inicio
            elif fase < 0.6:
                self.modelo.step(DT, 0.0)     # pausa
            elif fase < 0.9:
                self.modelo.step(DT, 0.15)    # ir a fin
            else:
                self.modelo.step(DT, 0.0)
        else:
            self.modelo.step(DT, 0.0)

        if elapsed >= duracion:
            if self._calibracion_tipo in ("angle", "full"):
                self.calibrado_angulo = True
            if self._calibracion_tipo in ("position", "full"):
                self.calibrado_posicion = True
            self.estado = "IDLE"
            logger.info("Calibración '%s' completada", self._calibracion_tipo)

    def _tick_swingup(self) -> None:
        self._swingup_t += DT
        angle = self.modelo.angle_deg

        # Estrategia de swing-up: impulsos en la dirección correcta
        # basados en la posición del péndulo
        if angle < 130 or angle > 230:
            # Péndulo lejos del equilibrio — aplicar impulsos de energía
            if angle < 180:
                fuerza = 0.6 * math.sin(self._swingup_t * 3.5)
            else:
                fuerza = -0.6 * math.sin(self._swingup_t * 3.5)
        else:
            # Dentro de ventana operativa → pasar a CONTROLLING
            self.estado = "CONTROLLING"
            self.pid_angulo.reset()
            self.pid_posicion.reset()
            logger.info("Swing-up completado → CONTROLLING (%.1f°)", angle)
            fuerza = 0.0

        self.modelo.step(DT, fuerza)

        # Timeout de swing-up: 30 segundos
        if self._swingup_t > 30.0:
            self.estado = "IDLE"
            self.errores.append("Timeout de swing-up")
            logger.warning("Timeout de swing-up — volviendo a IDLE")

    def _tick_controlling(self) -> None:
        angle = self.modelo.angle_deg

        if self.modo == "MANUAL":
            # Modo manual: aplicar PWM y dirección del comando
            fuerza = self._cmd_pwm / 1023.0
            if self._cmd_dir == "ATRAS":
                fuerza = -fuerza
            elif self._cmd_dir == "DETENIDO":
                fuerza = 0.0
            self.modelo.step(DT, fuerza)
            return

        # Verificar seguridad: si el ángulo sale de [130, 230] → emergencia
        if angle < 130 or angle > 230:
            self.estado = "EMERGENCY"
            self.errores.append("Ángulo fuera de rango de seguridad")
            logger.warning("Ángulo %.1f° fuera de rango → EMERGENCY", angle)
            return

        # PID cascada: posición → ajuste de setpoint de ángulo → PWM
        pos_error = 45.0 - self.modelo.position_cm
        pos_correccion = self.pid_posicion.compute(pos_error, DT)
        pos_correccion = max(-2.0, min(2.0, pos_correccion))

        angle_setpoint = 180.0 + pos_correccion
        angle_error = angle_setpoint - angle

        salida = self.pid_angulo.compute(angle_error, DT)
        fuerza = max(-1.0, min(1.0, salida / 1023.0))

        # Agregar perturbación aleatoria periódica (~cada 5s)
        if random.random() < DT / 5.0:
            perturbacion = random.gauss(0, 0.03)
            self.modelo.theta_d += perturbacion
            logger.debug("Perturbación aplicada: %.4f rad/s", perturbacion)

        self.modelo.step(DT, fuerza)
        self.modelo.pwm = abs(int(salida))
        self.modelo.direction = "ADELANTE" if salida > 0 else "ATRAS" if salida < 0 else "DETENIDO"

    def _agregar_ruido(self) -> None:
        """Ruido de sensor realista."""
        self._noise_angle = random.gauss(0, 0.5)     # ±0.5° ruido ángulo
        self._noise_pos = random.gauss(0, 0.2)        # ±0.2 cm ruido posición
        self._noise_accel = random.gauss(0, 0.05)     # ±0.05 m/s² ruido acelerómetro

    def _actualizar_salida(self) -> None:
        if self.estado in ("IDLE", "EMERGENCY", "STOPPING"):
            self.modelo.pwm = 0
            self.modelo.direction = "DETENIDO"

    # ── Generadores de payload MQTT ───────────────
    def payload_telemetria(self) -> dict:
        return {
            "angle": round(self.modelo.angle_deg + getattr(self, "_noise_angle", 0), 2),
            "position": round(self.modelo.position_cm + getattr(self, "_noise_pos", 0), 2),
            "cart_accel": round(self.modelo.cart_accel + getattr(self, "_noise_accel", 0), 3),
            "rpm": round(self.modelo.rpm, 1),
            "pwm": self.modelo.pwm,
            "direction": self.modelo.direction,
            "ts": self.millis,
        }

    def payload_estado(self) -> dict:
        return {
            "state": self.estado,
            "mode": self.modo,
            "limits": {
                "start_switch": self.modelo.limit_start,
                "end_switch": self.modelo.limit_end,
            },
            "errors": self.errores[-5:],  # últimos 5 errores
            "wifi_rssi": random.randint(-55, -35),
            "calibrated_angle": self.calibrado_angulo,
            "calibrated_position": self.calibrado_posicion,
        }

    def payload_limites(self) -> dict | None:
        """Retorna payload solo si cambió el estado de los límites."""
        actual = (self.modelo.limit_start, self.modelo.limit_end)
        if actual != self._prev_limits:
            self._prev_limits = actual
            return {
                "start_switch": actual[0],
                "end_switch": actual[1],
            }
        return None

    # ── Procesamiento de comandos ─────────────────
    def procesar_comando(self, topico: str, datos: dict) -> None:
        cmd = topico.split("/")[-1]  # motor, mode, calibrate, config, stop

        if cmd == "stop":
            logger.info("⚡ PARADA DE EMERGENCIA recibida")
            self.estado = "EMERGENCY"

        elif cmd == "motor":
            self._cmd_pwm = datos.get("pwm", 0)
            self._cmd_dir = datos.get("direction", "DETENIDO")
            action = datos.get("action", "set")
            if action == "brake":
                self._cmd_pwm = 0
                self._cmd_dir = "DETENIDO"
            logger.info("Motor: action=%s, pwm=%d, dir=%s", action, self._cmd_pwm, self._cmd_dir)

        elif cmd == "mode":
            nuevo_modo = datos.get("mode", "MANUAL")
            self.modo = nuevo_modo
            logger.info("Modo cambiado a: %s", nuevo_modo)

            # Si se pide iniciar (implícito con cambio de modo no-manual)
            if self.estado == "IDLE" and nuevo_modo != "MANUAL":
                self._swingup_t = 0
                self.estado = "SWING_UP"
                logger.info("Iniciando SWING_UP para modo %s", nuevo_modo)
            elif self.estado == "IDLE" and nuevo_modo == "MANUAL":
                self.estado = "CONTROLLING"

        elif cmd == "calibrate":
            tipo = datos.get("type", "full")
            self._calibracion_tipo = tipo
            self._calibracion_inicio = time.time()
            self._calibracion_progreso = 0
            self.estado = "CALIBRATING"
            logger.info("Iniciando calibración: %s", tipo)

        elif cmd == "config":
            param = datos.get("param", "")
            valor = datos.get("value", 0)
            logger.info("Config recibida: %s = %s", param, valor)
            # El mock no aplica parámetros realmente, solo los loguea

        else:
            logger.warning("Comando desconocido: %s", cmd)


# ══════════════════════════════════════════════════
#  Loop principal async
# ══════════════════════════════════════════════════

async def main() -> None:
    sim = SimuladorPendulo()
    logger.info("Simulador de péndulo invertido iniciado")
    logger.info("Conectando a MQTT %s:%d ...", MQTT_HOST, MQTT_PORT)

    espera = 1
    while True:
        try:
            async with aiomqtt.Client(
                hostname=MQTT_HOST,
                port=MQTT_PORT,
                identifier=f"pendulo-mock-{os.getpid()}",
            ) as client:
                espera = 1
                logger.info("Conectado al broker MQTT")
                await client.subscribe(T_CMD)

                cont_telemetria = 0
                periodo_estado = int(FREQ_TELEMETRIA / FREQ_ESTADO)

                async def loop_simulacion():
                    nonlocal cont_telemetria
                    while True:
                        sim.tick()

                        # Publicar telemetría a ~50 Hz
                        payload = orjson.dumps(sim.payload_telemetria())
                        await client.publish(T_TELEMETRIA, payload)

                        cont_telemetria += 1

                        # Publicar estado a ~2 Hz
                        if cont_telemetria % periodo_estado == 0:
                            estado_payload = orjson.dumps(sim.payload_estado())
                            await client.publish(T_ESTADO, estado_payload)

                        # Publicar límites on-change
                        lim_payload = sim.payload_limites()
                        if lim_payload:
                            await client.publish(T_LIMITES, orjson.dumps(lim_payload))

                        await asyncio.sleep(DT)

                async def loop_comandos():
                    async for msg in client.messages:
                        topico = str(msg.topic)
                        try:
                            datos = orjson.loads(msg.payload)
                        except (orjson.JSONDecodeError, TypeError):
                            datos = {}
                        sim.procesar_comando(topico, datos)

                # Ejecutar ambos loops en paralelo
                await asyncio.gather(
                    loop_simulacion(),
                    loop_comandos(),
                )

        except aiomqtt.MqttError as e:
            logger.warning("Conexión MQTT perdida: %s — reintentando en %ds", e, espera)
            await asyncio.sleep(espera)
            espera = min(espera * 2, 30)
        except KeyboardInterrupt:
            break

    logger.info("Simulador detenido")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
