<template>
  <div class="card">
    <div class="card-header">
      <span class="card-title">Resultados Lazo Abierto</span>
      <div class="card-header-actions">
        <span class="resultado-count">{{ registros.length }} muestras</span>
        <select v-model="maxFilasVisibles" class="select-filas">
          <option :value="500">500 filas</option>
          <option :value="1000">1000 filas</option>
          <option :value="2000">2000 filas</option>
        </select>
        <button class="btn btn-csv" @click="exportarCSV" :disabled="!registros.length">
          📥 CSV
        </button>
        <button class="btn btn-limpiar" @click="store.limpiarResultadosOpenLoop()" :disabled="!registros.length">
          🗑 Limpiar
        </button>
      </div>
    </div>

    <!-- Filtro de rango temporal para exportación -->
    <div class="rango-exportar">
      <span class="rango-label">Rango CSV:</span>
      <input type="datetime-local" v-model="rangoDesde" class="input-fecha" />
      <span class="rango-sep">→</span>
      <input type="datetime-local" v-model="rangoHasta" class="input-fecha" />
      <button v-if="rangoDesde || rangoHasta" class="btn-clear-rango" @click="limpiarRango">✕</button>
    </div>

    <div class="card-body">
      <!-- Tabla de datos -->
      <div class="tabla-wrapper">
        <table v-if="registros.length" class="tabla-resultados">
          <thead>
            <tr>
              <th>#</th>
              <th>Tiempo</th>
              <th>Estado</th>
              <th>Dir</th>
              <th>LS Ini</th>
              <th>LS Fin</th>
              <th>PWM</th>
              <th>Áng Pénd°</th>
              <th>Vel Ang°/s</th>
              <th>Pos IR</th>
              <th>RPM IR</th>
              <th>Pos MAG</th>
              <th>RPM MAG</th>
              <th>AX</th>
              <th>AY</th>
              <th>AZ</th>
              <th>Pitch°</th>
              <th>Roll°</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(r, i) in registrosVisibles" :key="i">
              <td class="mono">{{ i + 1 }}</td>
              <td class="mono">{{ formatTime(r.ts) }}</td>
              <td class="estado-cell">{{ r.state || '—' }}</td>
              <td>{{ dirTexto(r.direction) }}</td>
              <td class="ls-cell">{{ r.ls_start ? '✓' : '✗' }}</td>
              <td class="ls-cell">{{ r.ls_end ? '✓' : '✗' }}</td>
              <td class="mono">{{ r.pwm }}</td>
              <td class="mono">{{ fmt(r.angle_pend, 1) }}</td>
              <td class="mono">{{ fmt(r.vel_ang_pend, 1) }}</td>
              <td class="mono">{{ fmt(r.pos_ir_cm, 1) }}</td>
              <td class="mono">{{ fmt(r.rpm_ir, 0) }}</td>
              <td class="mono">{{ fmt(r.pos_mag_cm, 1) }}</td>
              <td class="mono">{{ fmt(r.rpm_mag, 0) }}</td>
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
import { ref, computed } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

// Estado local
const maxFilasVisibles = ref(500)
const rangoDesde = ref('')
const rangoHasta = ref('')

const registros = computed(() => store.openLoopResults)

// Filas visibles según selector
const registrosVisibles = computed(() => {
  const arr = registros.value
  const max = maxFilasVisibles.value
  return arr.length > max ? arr.slice(arr.length - max) : arr
})

const DIR_MAP = { 1: 'ADL', '-1': 'ATR', 0: 'DET' }
function dirTexto(d) { return DIR_MAP[d] ?? 'DET' }

function fmt(val, decimals) {
  return (val ?? 0).toFixed(decimals)
}

// Formato de tiempo en UTC-5 (Bogotá)
function formatTime(ts) {
  if (!ts) return '--'
  const d = new Date(ts)
  const opciones = {
    timeZone: 'America/Bogota',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false
  }
  const timeStr = d.toLocaleTimeString('es-CO', opciones)
  const ms = String(d.getMilliseconds()).padStart(3, '0')
  return `${timeStr}.${ms}`
}

function limpiarRango() {
  rangoDesde.value = ''
  rangoHasta.value = ''
}

// Filtrar registros según rango temporal para exportación
function filtrarPorRango(lista) {
  if (!rangoDesde.value && !rangoHasta.value) return lista
  
  let desde = rangoDesde.value ? new Date(rangoDesde.value).getTime() : 0
  let hasta = rangoHasta.value ? new Date(rangoHasta.value).getTime() : Infinity
  
  return lista.filter(r => r.ts >= desde && r.ts <= hasta)
}

function exportarCSV() {
  if (!registros.value.length) return

  const registrosFiltrados = filtrarPorRango(registros.value)
  if (!registrosFiltrados.length) {
    alert('No hay registros en el rango seleccionado')
    return
  }

  const headers = [
    '#', 'Timestamp', 'State', 'Direction',
    'LS_Start', 'LS_End', 'PWM',
    'Angle_Pend_deg', 'Vel_Ang_Pend_deg_s',
    'Pos_IR_cm', 'RPM_IR', 'RPS_IR',
    'Pos_Mag_cm', 'RPM_Mag',
    'MPU_ax', 'MPU_ay', 'MPU_az',
    'MPU_Pitch', 'MPU_Roll'
  ]

  const rows = registrosFiltrados.map((r, i) => [
    i + 1,
    r.ts ? new Date(r.ts).toISOString() : '',
    r.state ?? '',
    dirTexto(r.direction),
    r.ls_start ? 1 : 0,
    r.ls_end ? 1 : 0,
    r.pwm ?? 0,
    fmt(r.angle_pend, 3),
    fmt(r.vel_ang_pend, 3),
    fmt(r.pos_ir_cm, 3),
    fmt(r.rpm_ir, 2),
    fmt(r.rps_ir, 3),
    fmt(r.pos_mag_cm, 3),
    fmt(r.rpm_mag, 2),
    fmt(r.mpu_ax, 4),
    fmt(r.mpu_ay, 4),
    fmt(r.mpu_az, 4),
    fmt(r.mpu_pitch, 3),
    fmt(r.mpu_roll, 3),
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
  gap: 10px;
  flex-wrap: wrap;
}

.card-title {
  font-weight: 600;
  font-size: 0.95rem;
}

.card-header-actions {
  margin-left: auto;
  display: flex;
  align-items: center;
  gap: 8px;
}

.resultado-count {
  font-size: 0.75rem;
  color: var(--md-on-surface-secondary);
  font-weight: 500;
  background: #E8E0EC;
  padding: 4px 10px;
  border-radius: 12px;
}

.select-filas {
  font-size: 0.72rem;
  padding: 4px 8px;
  border: 1px solid var(--md-outline);
  border-radius: 6px;
  background: var(--md-surface);
  color: var(--md-on-surface);
  cursor: pointer;
}

.btn {
  border: none;
  border-radius: 6px;
  padding: 5px 10px;
  font-size: 0.72rem;
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

/* ── Rango de exportación ─────────────────────── */
.rango-exportar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 16px;
  background: #FAFAFA;
  border-bottom: 1px solid var(--md-outline);
  font-size: 0.72rem;
}

.rango-label {
  color: var(--md-on-surface-secondary);
  font-weight: 500;
}

.input-fecha {
  font-size: 0.7rem;
  padding: 4px 6px;
  border: 1px solid var(--md-outline);
  border-radius: 4px;
  background: var(--md-surface);
  color: var(--md-on-surface);
}

.rango-sep {
  color: var(--md-on-surface-secondary);
}

.btn-clear-rango {
  background: none;
  border: none;
  color: #C62828;
  cursor: pointer;
  font-size: 0.8rem;
  padding: 2px 6px;
  border-radius: 4px;
}

.btn-clear-rango:hover {
  background: #FFEBEE;
}

/* ── Tabla ────────────────────────────────────── */
.tabla-wrapper {
  max-height: 380px;
  overflow: auto;
  border: 1px solid var(--md-outline);
  border-radius: 8px;
}

.tabla-resultados {
  width: max-content;
  min-width: 100%;
  border-collapse: collapse;
  font-size: 0.68rem;
}

.tabla-resultados th {
  position: sticky;
  top: 0;
  background: var(--md-surface-dim);
  padding: 5px 6px;
  text-align: left;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  font-size: 0.6rem;
  text-transform: uppercase;
  letter-spacing: 0.02em;
  border-bottom: 2px solid var(--md-outline);
  white-space: nowrap;
  z-index: 1;
}

.tabla-resultados td {
  padding: 3px 6px;
  border-bottom: 1px solid #EDE8F0;
  color: var(--md-on-surface);
  white-space: nowrap;
}

.tabla-resultados tr:hover {
  background: #F8F4FA;
}

.mono {
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
  font-size: 0.66rem;
}

.estado-cell {
  font-size: 0.6rem;
  font-weight: 500;
  color: #6A1B9A;
}

.ls-cell {
  text-align: center;
  font-size: 0.7rem;
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
