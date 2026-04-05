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
    # Estados del modo Lazo Abierto
    OL_IDLE = "OL_IDLE"
    OL_HOMING_START = "OL_HOMING_START"
    OL_HOMING_END = "OL_HOMING_END"
    OL_READY = "OL_READY"
    OL_IMPULSE_RUNNING = "OL_IMPULSE_RUNNING"
    OL_STOPPED = "OL_STOPPED"
    # Intermedios de barrido
    SETTLE_END = "SETTLE_END"
    SETTLE_START = "SETTLE_START"
    PAUSE_AT_START = "PAUSE_AT_START"
    # Estados del modo PID Posición
    PID_IDLE = "PID_IDLE"
    PID_HOMING_START = "PID_HOMING_START"
    PID_HOMING_END = "PID_HOMING_END"
    PID_CALIBRATING = "PID_CALIBRATING"
    PID_RUNNING = "PID_RUNNING"
    PID_STOPPED = "PID_STOPPED"
    # Estados del modo PID Ángulo
    PIDA_IDLE = "PIDA_IDLE"
    PIDA_HOMING_START = "PIDA_HOMING_START"
    PIDA_HOMING_END = "PIDA_HOMING_END"
    PIDA_CALIBRATING = "PIDA_CALIBRATING"
    PIDA_PREPOSITIONING = "PIDA_PREPOSITIONING"
    PIDA_READY = "PIDA_READY"
    PIDA_RUNNING = "PIDA_RUNNING"
    PIDA_REPOSITIONING = "PIDA_REPOSITIONING"
    PIDA_STOPPED = "PIDA_STOPPED"


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
    OPEN_LOOP = "OPEN_LOOP"
    PID_POS = "PID_POS"
    PID_ANG = "PID_ANG"


class DireccionMotor(str, Enum):
    ADELANTE = "ADELANTE"
    ATRAS = "ATRAS"
    DETENIDO = "DETENIDO"


class TipoCalibracion(str, Enum):
    ANGULO = "angle"
    POSICION = "position"
    MPU = "mpu"
    COMPLETA = "full"


# ══════════════════════════════════════════════════
#  Datos: ESP32 → Backend
# ══════════════════════════════════════════════════

class Telemetria(BaseModel):
    """Paquete de telemetría a ~50 Hz — Todos los sensores."""
    # Campos de compatibilidad
    angle: float = Field(0.0, description="Ángulo del péndulo (grados)")
    position: float = Field(0.0, description="Posición del carro (cm)")
    cart_accel: float = Field(0.0, description="Aceleración del carro m/s²")
    rpm: float = Field(0.0, description="RPM del motor (encoder IR)")
    pwm: int = Field(0, description="Duty cycle PWM actual (0-1023)")
    direction: int = Field(0, description="Dirección motor: -1/0/1")
    ts: int = Field(0, description="Timestamp ESP32 (millis)")

    # Encoder IR (HC-020K)
    pulsos_ir: int = Field(0)
    vueltas_ir: float = Field(0.0)
    rps_ir: float = Field(0.0)
    rpm_ir: float = Field(0.0)
    pos_ir_cm: float = Field(0.0)

    # AS5600 #2 — Posición magnética del carro
    rps_mag: float = Field(0.0)
    rpm_mag: float = Field(0.0)
    pos_mag_cm: float = Field(0.0)

    # AS5600 #1 — Ángulo péndulo
    angle_pend: float = Field(0.0)
    vel_ang_pend: float = Field(0.0)

    # MPU6050
    mpu_ax: float = Field(0.0)
    mpu_ay: float = Field(0.0)
    mpu_az: float = Field(0.0)
    mpu_gx: float = Field(0.0)
    mpu_gy: float = Field(0.0)
    mpu_gz: float = Field(0.0)
    mpu_pitch: float = Field(0.0)
    mpu_roll: float = Field(0.0)

    # Limit switches
    ls_start: bool = Field(False)
    ls_end: bool = Field(False)

    # Modo del firmware
    modo: str = Field("", description="Modo activo del firmware")

    # Datos de barrido (compatibilidad)
    posicion: float = Field(0.0)
    direccion: int = Field(0)

    # ── PID Posición ──────────────────────────────
    pid_error: float = Field(0.0, description="Error de posición (setpoint - actual)")
    pid_integral: float = Field(0.0, description="Término integral acumulado")
    pid_derivada: float = Field(0.0, description="Término derivativo filtrado")
    pid_output: float = Field(0.0, description="Salida PID antes de saturación")
    pid_output_sat: float = Field(0.0, description="Salida PID saturada (PWM aplicado)")
    pid_setpoint: float = Field(0.0, description="Setpoint de posición (cm)")
    pid_kp: float = Field(0.0, description="Ganancia proporcional actual")
    pid_ki: float = Field(0.0, description="Ganancia integral actual")
    pid_kd: float = Field(0.0, description="Ganancia derivativa actual")
    pid_zona_seg: float = Field(0.0, description="Zona de seguridad (cm)")
    pid_tolerancia: float = Field(0.0, description="Tolerancia para considerar llegada (cm)")
    pid_sensor_type: int = Field(0, description="Tipo de sensor: 0=mag, 1=ir")
    pid_pwm_min: int = Field(0, description="PWM mínimo configurado")
    pid_pwm_max: int = Field(0, description="PWM máximo configurado")
    pid_state: str = Field("", description="Estado del modo PID: IDLE, RUNNING, etc.")

    # ── PID Ángulo ────────────────────────────────
    pida_error: float = Field(0.0, description="Error angular (setpoint - actual)")
    pida_integral: float = Field(0.0, description="Término integral acumulado")
    pida_derivada: float = Field(0.0, description="Término derivativo filtrado")
    pida_output: float = Field(0.0, description="Salida PID antes de saturación")
    pida_output_sat: float = Field(0.0, description="Salida PID saturada")
    pida_setpoint: float = Field(0.0, description="Setpoint angular (grados)")
    pida_kp: float = Field(0.0, description="Ganancia proporcional ángulo")
    pida_ki: float = Field(0.0, description="Ganancia integral ángulo")
    pida_kd: float = Field(0.0, description="Ganancia derivativa ángulo")
    pida_rango_trabajo: float = Field(0.0, description="Rango de trabajo (grados)")
    pida_tolerancia: float = Field(0.0, description="Tolerancia angular (grados)")
    pida_pwm_min: int = Field(0, description="PWM mínimo ángulo")
    pida_pwm_max: int = Field(0, description="PWM máximo ángulo")
    pida_pos_setpoint: float = Field(0.0, description="Setpoint posición interna (cm)")

    # ── PID Posición interno (dentro de PID Ángulo) ───
    pida_pos_error: float = Field(0.0, description="Error de posición interno (cm)")
    pida_pos_integral: float = Field(0.0, description="Integral de posición interno")
    pida_pos_derivada: float = Field(0.0, description="Derivada de posición interno")
    pida_pos_output: float = Field(0.0, description="Salida PID posición interno")


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
    """Comando para el barrido PWM."""
    action: str = Field(..., description="'start', 'stop', 'reiniciar'")


class ComandoOpenLoop(BaseModel):
    """Comando para el modo Lazo Abierto."""
    action: str = Field(..., description="'home', 'end', 'impulse', 'stop'")
    pwm: int = Field(500, ge=200, le=1023)
    duration: float = Field(1.0, ge=0.1, le=3.0)


class ComandoSweep(BaseModel):
    """Control del barrido de rango operativo PWM."""
    action: str = Field("start", description="'start' para iniciar/reiniciar barrido")


class ComandoPidPos(BaseModel):
    """Comando para el modo PID de Posición."""
    action: str = Field(..., description="'home', 'end', 'config', 'run', 'stop', 'reset'")
    kp: float | None = Field(None, ge=0.1, le=50.0, description="Ganancia proporcional")
    ki: float | None = Field(None, ge=0.0, le=10.0, description="Ganancia integral")
    kd: float | None = Field(None, ge=0.0, le=5.0, description="Ganancia derivativa")
    setpoint: float | None = Field(None, description="Posición objetivo (cm)")
    zona_seguridad: float | None = Field(None, ge=2.0, le=20.0, description="Zona segura desde límites (cm)")
    tolerancia: float | None = Field(None, ge=0.5, le=5.0, description="Tolerancia para llegada (cm)")
    sensor: str | None = Field(None, description="Sensor de posición: 'mag' o 'ir'")
    pwm_min: int | None = Field(None, ge=100, le=500, description="PWM mínimo")
    pwm_max: int | None = Field(None, ge=300, le=1023, description="PWM máximo")


class ComandoPidAng(BaseModel):
    """Comando para el modo PID de Ángulo."""
    action: str = Field(..., description="'home', 'end', 'config', 'run', 'stop', 'reset'")
    kp: float | None = Field(None, ge=0.1, le=50.0, description="Ganancia proporcional")
    ki: float | None = Field(None, ge=0.0, le=10.0, description="Ganancia integral")
    kd: float | None = Field(None, ge=0.0, le=5.0, description="Ganancia derivativa")
    rango_trabajo: float | None = Field(None, ge=5.0, le=30.0, description="Rango de trabajo (grados)")
    tolerancia: float | None = Field(None, ge=1.0, le=10.0, description="Tolerancia angular (grados)")
    pwm_min: int | None = Field(None, ge=100, le=500, description="PWM mínimo")
    pwm_max: int | None = Field(None, ge=300, le=1023, description="PWM máximo")


# ══════════════════════════════════════════════════
#  WebSocket: mensajes bidireccionales
# ══════════════════════════════════════════════════

class MensajeWS(BaseModel):
    """Envoltorio genérico para mensajes WebSocket."""
    tipo: str = Field(..., description="Tipo: 'telemetria', 'estado', 'limites', 'comando'")
    datos: dict[str, Any] = Field(default_factory=dict)
