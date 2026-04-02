<template>
  <div class="indicadores-container">

    <!-- ═══════ SECCIÓN: PÉNDULO ═══════ -->
    <section class="seccion seccion-pendulo">
      <h4 class="seccion-titulo">Péndulo</h4>
      <div class="indicadores-row">
        <div class="indicador indicador-destacado">
          <span class="indicador-label">Ángulo</span>
          <span class="indicador-valor-lg">{{ fmt(t.angle_pend, 1) }}°</span>
        </div>
        <div class="indicador indicador-destacado">
          <span class="indicador-label">Vel. Angular</span>
          <span class="indicador-valor-lg">{{ fmt(t.vel_ang_pend, 1) }}</span>
          <span class="indicador-unidad">°/s</span>
        </div>
      </div>
    </section>

    <!-- ═══════ SECCIÓN: CARRO PRINCIPAL ═══════ -->
    <section class="seccion seccion-carro">
      <h4 class="seccion-titulo">Carro</h4>
      <div class="indicadores-row indicadores-4col">
        <div class="indicador">
          <span class="indicador-label">Pos IR</span>
          <span class="indicador-valor">{{ fmt(t.pos_ir_cm, 1) }}</span>
          <span class="indicador-unidad">cm</span>
        </div>
        <div class="indicador">
          <span class="indicador-label">RPM IR</span>
          <span class="indicador-valor">{{ fmt(t.rpm_ir, 0) }}</span>
        </div>
        <div class="indicador">
          <span class="indicador-label">Pos Mag</span>
          <span class="indicador-valor">{{ fmt(t.pos_mag_cm, 1) }}</span>
          <span class="indicador-unidad">cm</span>
        </div>
        <div class="indicador">
          <span class="indicador-label">RPM Mag</span>
          <span class="indicador-valor">{{ fmt(t.rpm_mag, 0) }}</span>
        </div>
      </div>
    </section>

    <!-- ═══════ SECCIÓN: ACELERÓMETRO (MPU) ═══════ -->
    <section class="seccion seccion-mpu">
      <h4 class="seccion-titulo">Acelerómetro</h4>
      <div class="indicadores-row indicadores-5col">
        <div class="indicador indicador-sm">
          <span class="indicador-label">AX</span>
          <span class="indicador-valor-sm">{{ fmt(t.mpu_ax, 2) }}</span>
        </div>
        <div class="indicador indicador-sm">
          <span class="indicador-label">AY</span>
          <span class="indicador-valor-sm">{{ fmt(t.mpu_ay, 2) }}</span>
        </div>
        <div class="indicador indicador-sm">
          <span class="indicador-label">AZ</span>
          <span class="indicador-valor-sm">{{ fmt(t.mpu_az, 2) }}</span>
        </div>
        <div class="indicador indicador-sm">
          <span class="indicador-label">Pitch</span>
          <span class="indicador-valor-sm">{{ fmt(t.mpu_pitch, 1) }}°</span>
        </div>
        <div class="indicador indicador-sm">
          <span class="indicador-label">Roll</span>
          <span class="indicador-valor-sm">{{ fmt(t.mpu_roll, 1) }}°</span>
        </div>
      </div>
    </section>

    <!-- ═══════ SECCIÓN: CONTROL ═══════ -->
    <section class="seccion seccion-control">
      <h4 class="seccion-titulo">Control</h4>
      <div class="indicadores-row indicadores-4col">
        <!-- Estado del sistema -->
        <div class="indicador">
          <span class="indicador-label">Estado</span>
          <span class="badge" :class="claseBadgeEstado">{{ estadoTexto }}</span>
        </div>

        <!-- PWM con barra (usa pid_output_sat si modo PID, sino pwm general) -->
        <div class="indicador">
          <span class="indicador-label">PWM</span>
          <span class="indicador-valor">{{ valorPwmMostrado }}</span>
          <div class="barra-pwm">
            <div
              class="barra-pwm-fill"
              :style="{ width: (Math.abs(valorPwmMostrado) / 1023 * 100) + '%' }"
            ></div>
          </div>
        </div>

        <!-- Dirección -->
        <div class="indicador">
          <span class="indicador-label">Dirección</span>
          <span class="badge" :class="claseBadgeDireccion">{{ textoDir }}</span>
        </div>

        <!-- Limit Switches -->
        <div class="indicador">
          <span class="indicador-label">Limit Switches</span>
          <div class="ls-row">
            <span class="ls-led" :class="t.ls_start ? 'ls-activo' : 'ls-ok'"></span>
            <span class="ls-texto">Ini</span>
            <span class="ls-led ls-led-right" :class="t.ls_end ? 'ls-activo' : 'ls-ok'"></span>
            <span class="ls-texto">Fin</span>
          </div>
        </div>
      </div>
    </section>

  </div>
</template>

<script setup>
import { computed } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()
const t = computed(() => store.telemetria)

// ── PWM mostrado según modo ──
// En modo PID usa pid_output (salida cruda del controlador), sino el pwm general
const valorPwmMostrado = computed(() => {
  const modo = t.value.modo || store.estado.mode || ''
  if (modo === 'PID_POS') {
    return Math.round(t.value.pid_output ?? 0)
  }
  return t.value.pwm ?? 0
})

// ── Formateo de valores ──
function fmt(val, decimals) {
  return (val ?? 0).toFixed(decimals)
}

// ── Estado del sistema ──
const ESTADO_TEXTO = {
  IDLE: 'INACTIVO',
  CALIBRATING: 'CALIBRANDO',
  READY: 'LISTO',
  SWING_UP: 'SWING UP',
  CONTROLLING: 'CONTROLANDO',
  STOPPING: 'DETENIENDO',
  EMERGENCY: 'EMERGENCIA',
}

const estadoTexto = computed(() => {
  const st = store.estado.state
  return ESTADO_TEXTO[st] ?? st ?? 'INACTIVO'
})

const claseBadgeEstado = computed(() => {
  const mapa = {
    IDLE: 'badge-idle',
    CALIBRATING: 'badge-calibrando',
    READY: 'badge-listo',
    SWING_UP: 'badge-swingup',
    CONTROLLING: 'badge-controlando',
    STOPPING: 'badge-deteniendo',
    EMERGENCY: 'badge-emergencia',
  }
  return mapa[store.estado.state] || 'badge-idle'
})

// ── Dirección del motor ──
const DIRECTION_TEXT = { 1: 'ADELANTE', '-1': 'ATRÁS', 0: 'DETENIDO' }
const textoDir = computed(() => {
  const d = t.value.direction
  if (typeof d === 'string') return d
  return DIRECTION_TEXT[d] ?? 'DETENIDO'
})

const claseBadgeDireccion = computed(() => {
  const d = t.value.direction
  if (d === 1 || d === 'ADELANTE') return 'badge-adelante'
  if (d === -1 || d === 'ATRAS' || d === 'ATRÁS') return 'badge-atras'
  return 'badge-idle'
})
</script>

<style scoped>
.indicadores-container {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

/* ── Secciones ── */
.seccion {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.seccion-titulo {
  margin: 0;
  font-size: 0.7rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--md-on-surface-secondary, #6E5F72);
  padding-bottom: 4px;
  border-bottom: 1px solid var(--md-outline-variant, #E0E0E0);
}

/* Colores por sección */
.seccion-pendulo .seccion-titulo { color: #8C2981; border-color: #E8D5E6; }
.seccion-carro .seccion-titulo { color: #1565C0; border-color: #BBDEFB; }
.seccion-mpu .seccion-titulo { color: #6A7C4E; border-color: #DCEDC8; }
.seccion-control .seccion-titulo { color: #5D4037; border-color: #D7CCC8; }

/* ── Filas de indicadores ── */
.indicadores-row {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 8px;
}

.indicadores-4col {
  grid-template-columns: repeat(4, 1fr);
}

.indicadores-5col {
  grid-template-columns: repeat(5, 1fr);
}

@media (max-width: 600px) {
  .indicadores-4col,
  .indicadores-5col {
    grid-template-columns: repeat(2, 1fr);
  }
}

/* ── Indicadores base ── */
.indicador {
  padding: 8px 10px;
  background: var(--md-surface-dim, #F5F5F5);
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.indicador-destacado {
  background: linear-gradient(135deg, #F3E8F9 0%, #FCE4EC 100%);
  text-align: center;
  padding: 10px 12px;
}

.indicador-sm {
  padding: 6px 8px;
}

.indicador-label {
  font-size: 0.65rem;
  color: var(--md-on-surface-secondary, #757575);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  font-weight: 600;
}

.indicador-valor-lg {
  font-size: 1.5rem;
  font-weight: 700;
  color: #8C2981;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
  line-height: 1.1;
}

.indicador-valor {
  font-size: 1rem;
  font-weight: 700;
  color: var(--md-primary, #1565C0);
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.indicador-valor-sm {
  font-size: 0.85rem;
  font-weight: 600;
  color: var(--md-on-surface, #333);
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.indicador-unidad {
  font-size: 0.6rem;
  color: var(--md-on-surface-secondary, #9E9E9E);
  margin-top: -2px;
}

/* ── Barra PWM ── */
.barra-pwm {
  height: 5px;
  background: var(--md-outline, #E0E0E0);
  border-radius: 3px;
  overflow: hidden;
  margin-top: 3px;
}

.barra-pwm-fill {
  height: 100%;
  background: linear-gradient(90deg, #51127C, #B5367A);
  border-radius: 3px;
  transition: width 0.1s ease;
}

/* ── Badges ── */
.badge {
  display: inline-block;
  padding: 2px 8px;
  border-radius: 10px;
  font-size: 0.68rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  width: fit-content;
}

.badge-idle { background: #E8E0EC; color: #6E5F72; }
.badge-calibrando { background: #FFF3E0; color: #E65100; }
.badge-listo { background: #E8F5E9; color: #2E7D32; }
.badge-swingup { background: #E3F2FD; color: #1565C0; }
.badge-controlando { background: #E8F5E9; color: #2E7D32; }
.badge-deteniendo { background: #FFF3E0; color: #E65100; }
.badge-emergencia {
  background: #FFEBEE;
  color: #C62828;
  animation: pulsar 1s ease-in-out infinite;
}
.badge-adelante { background: #E8F5E9; color: #2E7D32; }
.badge-atras { background: #FDECDA; color: #C4501A; }

@keyframes pulsar {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

/* ── Limit Switches ── */
.ls-row {
  display: flex;
  align-items: center;
  gap: 4px;
  margin-top: 2px;
}

.ls-led {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  display: inline-block;
}

.ls-led-right {
  margin-left: 8px;
}

.ls-ok {
  background: #43A047;
  box-shadow: 0 0 3px #43A047;
}

.ls-activo {
  background: #F44336;
  box-shadow: 0 0 6px #F44336;
  animation: pulsar 0.5s ease-in-out infinite;
}

.ls-texto {
  font-size: 0.6rem;
  color: var(--md-on-surface-secondary, #757575);
  font-weight: 500;
}
</style>
