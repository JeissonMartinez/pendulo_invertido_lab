<template>
  <div class="card">
    <div class="card-header">
      Resultados Lazo Abierto
      <span class="resultado-count">{{ registros.length }} muestras</span>
      <div class="card-header-actions">
        <button class="btn btn-csv" @click="exportarCSV" :disabled="!registros.length">
          📥 Exportar CSV
        </button>
        <button class="btn btn-limpiar" @click="store.limpiarResultadosOpenLoop()" :disabled="!registros.length">
          🗑 Limpiar
        </button>
      </div>
    </div>

    <div class="card-body">
      <!-- Resumen rápido -->
      <div v-if="registros.length" class="resumen-grid">
        <div class="resumen-item resumen-muestras">
          <span class="resumen-label">Muestras</span>
          <span class="resumen-valor">{{ registros.length }}</span>
        </div>
        <div class="resumen-item resumen-angulo">
          <span class="resumen-label">Ángulo Pénd (últ)</span>
          <span class="resumen-valor">{{ fmt(ultimoRegistro.angle_pend, 1) }}°</span>
        </div>
        <div class="resumen-item resumen-pos">
          <span class="resumen-label">Pos Mag (últ)</span>
          <span class="resumen-valor">{{ fmt(ultimoRegistro.pos_mag_cm, 1) }} cm</span>
        </div>
        <div class="resumen-item resumen-rpm">
          <span class="resumen-label">RPM IR (últ)</span>
          <span class="resumen-valor">{{ fmt(ultimoRegistro.rpm_ir, 0) }}</span>
        </div>
      </div>

      <!-- Tabla de datos -->
      <div class="tabla-wrapper">
        <table v-if="registros.length" class="tabla-resultados">
          <thead>
            <tr>
              <th>#</th>
              <th>Tiempo</th>
              <th>PWM</th>
              <th>Dir</th>
              <th>Pos IR cm</th>
              <th>Pos Mag cm</th>
              <th>Ángulo°</th>
              <th>Vel Ang°/s</th>
              <th>RPM IR</th>
              <th>ax</th>
              <th>ay</th>
              <th>az</th>
              <th>Pitch°</th>
              <th>Roll°</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(r, i) in registrosVisibles" :key="i">
              <td class="mono">{{ i + 1 }}</td>
              <td class="mono">{{ formatTime(r.ts) }}</td>
              <td class="mono">{{ r.pwm }}</td>
              <td>{{ dirTexto(r.direction) }}</td>
              <td class="mono">{{ fmt(r.pos_ir_cm, 1) }}</td>
              <td class="mono">{{ fmt(r.pos_mag_cm, 1) }}</td>
              <td class="mono">{{ fmt(r.angle_pend, 1) }}</td>
              <td class="mono">{{ fmt(r.vel_ang_pend, 1) }}</td>
              <td class="mono">{{ fmt(r.rpm_ir, 0) }}</td>
              <td class="mono">{{ fmt(r.mpu_ax, 2) }}</td>
              <td class="mono">{{ fmt(r.mpu_ay, 2) }}</td>
              <td class="mono">{{ fmt(r.mpu_az, 2) }}</td>
              <td class="mono">{{ fmt(r.mpu_pitch, 1) }}</td>
              <td class="mono">{{ fmt(r.mpu_roll, 1) }}</td>
            </tr>
          </tbody>
        </table>
        <div v-else class="sin-datos">
          <p>No hay datos registrados aún.</p>
          <p class="sin-datos-sub">Los datos se capturan durante impulsos en lazo abierto.</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const registros = computed(() => store.openLoopResults)
const ultimoRegistro = computed(() => registros.value[registros.value.length - 1] ?? {})

// Show last 500 rows in real-time table to avoid DOM overload
const registrosVisibles = computed(() => {
  const arr = registros.value
  return arr.length > 500 ? arr.slice(arr.length - 500) : arr
})

const DIR_MAP = { 1: 'ADL', '-1': 'ATR', 0: 'DET' }
function dirTexto(d) { return DIR_MAP[d] ?? 'DET' }

function fmt(val, decimals) {
  return (val ?? 0).toFixed(decimals)
}

function formatTime(ts) {
  if (!ts) return '--'
  const d = new Date(ts)
  const m = String(d.getMinutes()).padStart(2, '0')
  const s = String(d.getSeconds()).padStart(2, '0')
  const ms = String(d.getMilliseconds()).padStart(3, '0')
  return `${m}:${s}.${ms}`
}

function exportarCSV() {
  if (!registros.value.length) return

  const headers = [
    '#', 'Timestamp', 'PWM', 'Direction',
    'Pos_IR_cm', 'Pos_Mag_cm', 'Angle_Pend_deg', 'Vel_Ang_Pend_deg_s',
    'RPM_IR', 'RPS_IR', 'RPM_Mag',
    'MPU_ax', 'MPU_ay', 'MPU_az',
    'MPU_Pitch', 'MPU_Roll',
    'LS_Start', 'LS_End'
  ]

  const rows = registros.value.map((r, i) => [
    i + 1,
    r.ts ? new Date(r.ts).toISOString() : '',
    r.pwm ?? 0,
    dirTexto(r.direction),
    fmt(r.pos_ir_cm, 3),
    fmt(r.pos_mag_cm, 3),
    fmt(r.angle_pend, 3),
    fmt(r.vel_ang_pend, 3),
    fmt(r.rpm_ir, 2),
    fmt(r.rps_ir, 3),
    fmt(r.rpm_mag, 2),
    fmt(r.mpu_ax, 4),
    fmt(r.mpu_ay, 4),
    fmt(r.mpu_az, 4),
    fmt(r.mpu_pitch, 3),
    fmt(r.mpu_roll, 3),
    r.ls_start ? 1 : 0,
    r.ls_end ? 1 : 0,
  ])

  const csv = [headers.join(','), ...rows.map(r => r.join(','))].join('\n')
  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = `lazo_abierto_${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.csv`
  a.click()
  URL.revokeObjectURL(url)
}
</script>

<style scoped>
.card-header {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
}

.resultado-count {
  font-size: 0.75rem;
  color: var(--md-on-surface-secondary);
  font-weight: 400;
}

.card-header-actions {
  margin-left: auto;
  display: flex;
  gap: 8px;
}

.btn {
  border: none;
  border-radius: 6px;
  padding: 6px 12px;
  font-size: 0.78rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
}

.btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.btn-csv {
  background: #E8F5E9;
  color: #2E7D32;
}

.btn-csv:hover:not(:disabled) {
  background: #C8E6C9;
}

.btn-limpiar {
  background: #FFEBEE;
  color: #C62828;
}

.btn-limpiar:hover:not(:disabled) {
  background: #FFCDD2;
}

/* ── Resumen ─────────────────────────────────── */
.resumen-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 10px;
  margin-bottom: 12px;
}

.resumen-item {
  padding: 12px;
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.resumen-muestras { background: #E8E0EC; }
.resumen-angulo { background: #F3E8F9; }
.resumen-pos { background: #E8F5E9; }
.resumen-rpm { background: #FDECDA; }

.resumen-label {
  font-size: 0.7rem;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  font-weight: 600;
}

.resumen-valor {
  font-size: 1.4rem;
  font-weight: 700;
  color: #8C2981;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.resumen-pos .resumen-valor { color: #2E7D32; }
.resumen-rpm .resumen-valor { color: #C4501A; }

/* ── Tabla ────────────────────────────────────── */
.tabla-wrapper {
  max-height: 380px;
  overflow-y: auto;
  border: 1px solid var(--md-outline);
  border-radius: 8px;
}

.tabla-resultados {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.74rem;
}

.tabla-resultados th {
  position: sticky;
  top: 0;
  background: var(--md-surface-dim);
  padding: 7px 8px;
  text-align: left;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  font-size: 0.66rem;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  border-bottom: 2px solid var(--md-outline);
  white-space: nowrap;
}

.tabla-resultados td {
  padding: 5px 8px;
  border-bottom: 1px solid #EDE8F0;
  color: var(--md-on-surface);
  white-space: nowrap;
}

.tabla-resultados tr:hover {
  background: #F8F4FA;
}

.mono {
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
  font-size: 0.72rem;
}

.sin-datos {
  text-align: center;
  padding: 40px;
  color: var(--md-on-surface-secondary);
}

.sin-datos-sub {
  font-size: 0.78rem;
  margin-top: 4px;
  opacity: 0.7;
}
</style>
