"""
Schemas Pydantic — Modelos de datos para telemetría, estado y comandos.
Reflejan exactamente los JSON que viajan por MQTT entre el ESP32 (o mock) y el backend.
"""

from __future__ import annotations
from enum import Enum
from typing import Any
from pydantic import BaseModel, Field


# ══════════════════════════════════════════════════
#  Enums
# ══════════════════════════════════════════════════

class EstadoSistema(str, Enum):
    IDLE = "IDLE"
    CALIBRATING = "CALIBRATING"
    SWING_UP = "SWING_UP"
    CONTROLLING = "CONTROLLING"
    STOPPING = "STOPPING"
    EMERGENCY = "EMERGENCY"
    # Estados del barrido PWM
    HOMING = "HOMING"
    READY = "READY"
    FWD_RUNNING = "FWD_RUNNING"
    PAUSE_AT_END = "PAUSE_AT_END"
    BACK_RUNNING = "BACK_RUNNING"
    SHOW_RESULTS = "SHOW_RESULTS"
    WAIT_REINICIAR = "WAIT_REINICIAR"


class ModoControl(str, Enum):
    MANUAL = "MANUAL"
    PID = "PID"
    NEURAL = "NEURAL"
    SMC = "SMC"
    LQR = "LQR"
    FUZZY = "FUZZY"
    MRAC = "MRAC"
    MPC = "MPC"
    SWEEP_PWM = "SWEEP_PWM"


class DireccionMotor(str, Enum):
    ADELANTE = "ADELANTE"
    ATRAS = "ATRAS"
    DETENIDO = "DETENIDO"


class TipoCalibracion(str, Enum):
    ANGULO = "angle"
    POSICION = "position"
    COMPLETA = "full"


# ══════════════════════════════════════════════════
#  Datos: ESP32 → Backend
# ══════════════════════════════════════════════════

class Telemetria(BaseModel):
    """Paquete de telemetría a ~50 Hz."""
    angle: float = Field(..., description="Ángulo del péndulo (grados, 180=vertical)")
    position: float = Field(..., description="Posición del carro (cm)")
    cart_accel: float = Field(0.0, description="Aceleración del carro m/s² (MPU6050)")
    rpm: float = Field(0.0, description="RPM del motor (encoder IR)")
    pwm: int = Field(0, description="Duty cycle PWM actual (0-1023)")
    direction: DireccionMotor = DireccionMotor.DETENIDO
    ts: int = Field(..., description="Timestamp ESP32 (millis)")


class EstadoLimites(BaseModel):
    """Estado de limit switches."""
    start_switch: bool = Field(False, description="True = activado (carro en extremo inicio)")
    end_switch: bool = Field(False, description="True = activado (carro en extremo fin)")


class Estado(BaseModel):
    """Estado general del sistema a ~2 Hz."""
    state: EstadoSistema = EstadoSistema.IDLE
    mode: ModoControl = ModoControl.MANUAL
    limits: EstadoLimites = Field(default_factory=EstadoLimites)
    errors: list[str] = Field(default_factory=list)
    wifi_rssi: int = Field(0, description="RSSI WiFi en dBm")
    calibrated_angle: bool = False
    calibrated_position: bool = False


# ══════════════════════════════════════════════════
#  Comandos: Backend → ESP32
# ══════════════════════════════════════════════════

class ComandoMotor(BaseModel):
    """Control directo del motor (solo modo Manual)."""
    action: str = Field("set", description="'set' para fijar PWM, 'brake' para frenar")
    pwm: int = Field(0, ge=0, le=1023)
    direction: DireccionMotor = DireccionMotor.DETENIDO


class ComandoModo(BaseModel):
    """Cambiar modo de control."""
    mode: ModoControl
    params: dict[str, Any] = Field(default_factory=dict)


class ComandoCalibracion(BaseModel):
    """Iniciar rutina de calibración."""
    type: TipoCalibracion


class ComandoConfig(BaseModel):
    """Modificar un parámetro del sistema en runtime."""
    param: str = Field(..., description="Nombre del parámetro (ej: 'KP_POS')")
    value: float


class ComandoStop(BaseModel):
    """Parada de emergencia."""
    pass


class ComandoSweep(BaseModel):
    """Control del barrido de rango operativo PWM."""
    action: str = Field("start", description="'start' para iniciar/reiniciar barrido")


# ══════════════════════════════════════════════════
#  WebSocket: mensajes bidireccionales
# ══════════════════════════════════════════════════

class MensajeWS(BaseModel):
    """Envoltorio genérico para mensajes WebSocket."""
    tipo: str = Field(..., description="Tipo: 'telemetria', 'estado', 'limites', 'comando'")
    datos: dict[str, Any] = Field(default_factory=dict)
