<template>
  <div class="indicadores-grid">
    <!-- Encoder IR -->
    <div class="indicador">
      <span class="indicador-label">RPM IR</span>
      <span class="indicador-valor">{{ fmt(t.rpm_ir, 0) }}</span>
    </div>
    <div class="indicador">
      <span class="indicador-label">RPS IR</span>
      <span class="indicador-valor-sm">{{ fmt(t.rps_ir, 2) }}</span>
    </div>
    <div class="indicador">
      <span class="indicador-label">Pos IR</span>
      <span class="indicador-valor-sm">{{ fmt(t.pos_ir_cm, 1) }} cm</span>
    </div>

    <!-- AS5600 Magnético -->
    <div class="indicador">
      <span class="indicador-label">RPM Mag</span>
      <span class="indicador-valor">{{ fmt(t.rpm_mag, 0) }}</span>
    </div>
    <div class="indicador">
      <span class="indicador-label">Pos Mag</span>
      <span class="indicador-valor-sm">{{ fmt(t.pos_mag_cm, 1) }} cm</span>
    </div>

    <!-- Péndulo -->
    <div class="indicador indicador-highlight">
      <span class="indicador-label">Ángulo Pénd</span>
      <span class="indicador-valor">{{ fmt(t.angle_pend, 1) }}°</span>
    </div>
    <div class="indicador">
      <span class="indicador-label">Vel Ang Pénd</span>
      <span class="indicador-valor-sm">{{ fmt(t.vel_ang_pend, 1) }} °/s</span>
    </div>

    <!-- MPU6050 Acelerómetro -->
    <div class="indicador">
      <span class="indicador-label">MPU ax</span>
      <span class="indicador-valor-sm">{{ fmt(t.mpu_ax, 2) }} m/s²</span>
    </div>
    <div class="indicador">
      <span class="indicador-label">MPU ay</span>
      <span class="indicador-valor-sm">{{ fmt(t.mpu_ay, 2) }} m/s²</span>
    </div>
    <div class="indicador">
      <span class="indicador-label">MPU az</span>
      <span class="indicador-valor-sm">{{ fmt(t.mpu_az, 2) }} m/s²</span>
    </div>

    <!-- MPU6050 Orientación -->
    <div class="indicador">
      <span class="indicador-label">MPU Pitch</span>
      <span class="indicador-valor-sm">{{ fmt(t.mpu_pitch, 1) }}°</span>
    </div>
    <div class="indicador">
      <span class="indicador-label">MPU Roll</span>
      <span class="indicador-valor-sm">{{ fmt(t.mpu_roll, 1) }}°</span>
    </div>

    <!-- PWM y Dirección -->
    <div class="indicador">
      <span class="indicador-label">PWM</span>
      <span class="indicador-valor">{{ t.pwm }}</span>
      <div class="barra-pwm">
        <div class="barra-pwm-fill" :style="{ width: (Math.abs(t.pwm) / 1023 * 100) + '%' }"></div>
      </div>
    </div>
    <div class="indicador">
      <span class="indicador-label">Dirección</span>
      <span class="badge" :class="claseBadgeDireccion">{{ textoDir }}</span>
    </div>

    <!-- Limit Switches -->
    <div class="indicador">
      <span class="indicador-label">Limit Switches</span>
      <div class="ls-row">
        <span class="ls-led" :class="t.ls_start ? 'ls-activo' : 'ls-ok'"></span>
        <span class="ls-texto">Inicio</span>
        <span class="ls-led" :class="t.ls_end ? 'ls-activo' : 'ls-ok'" style="margin-left: 12px;"></span>
        <span class="ls-texto">Fin</span>
      </div>
    </div>

    <!-- WiFi -->
    <div class="indicador">
      <span class="indicador-label">WiFi RSSI</span>
      <span class="indicador-valor-sm">{{ store.estado.wifi_rssi }} dBm</span>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()
const t = computed(() => store.telemetria)

const DIRECTION_TEXT = { 1: 'ADELANTE', '-1': 'ATRAS', 0: 'DETENIDO' }
const textoDir = computed(() => DIRECTION_TEXT[t.value.direction] ?? 'DETENIDO')

const claseBadgeDireccion = computed(() => {
  const d = t.value.direction
  if (d === 1) return 'badge-adelante'
  if (d === -1) return 'badge-atras'
  return 'badge-idle'
})

function fmt(val, decimals) {
  return (val ?? 0).toFixed(decimals)
}
</script>

<style scoped>
.indicadores-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: 8px;
}

.indicador {
  padding: 8px 10px;
  background: var(--md-surface-dim);
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  gap: 3px;
}

.indicador-highlight {
  background: #F3E8F9;
}

.indicador-label {
  font-size: 0.68rem;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  font-weight: 600;
}

.indicador-valor {
  font-size: 1.15rem;
  font-weight: 700;
  color: #8C2981;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.indicador-valor-sm {
  font-size: 0.88rem;
  font-weight: 600;
  color: var(--md-on-surface);
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.barra-pwm {
  height: 5px;
  background: var(--md-outline);
  border-radius: 3px;
  overflow: hidden;
  margin-top: 2px;
}

.barra-pwm-fill {
  height: 100%;
  background: linear-gradient(90deg, #51127C, #B5367A);
  border-radius: 3px;
  transition: width 0.1s ease;
}

.badge {
  display: inline-block;
  padding: 2px 8px;
  border-radius: 10px;
  font-size: 0.7rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  width: fit-content;
}

.badge-idle { background: #E8E0EC; color: #6E5F72; }
.badge-adelante { background: #E8F5E9; color: #2E7D32; }
.badge-atras { background: #FDECDA; color: #C4501A; }

.ls-row {
  display: flex;
  align-items: center;
  gap: 4px;
}

.ls-led {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  display: inline-block;
}

.ls-ok { background: #43A047; }
.ls-activo { background: #E53935; animation: pulsar 0.8s infinite; }
.ls-texto { font-size: 0.68rem; color: var(--md-on-surface-secondary); }

@keyframes pulsar {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}
</style>
