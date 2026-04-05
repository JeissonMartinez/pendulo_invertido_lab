<template>
  <div class="card">
    <div class="card-header">
      <span class="card-title">Resultados PID Ángulo</span>
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
        <button class="btn btn-limpiar" @click="store.limpiarResultadosPidAng()" :disabled="!registros.length">
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
              <!-- Posición -->
              <th>Pos IR</th>
              <th>Pos MAG</th>
              <th>RPM IR</th>
              <th>RPM MAG</th>
              <!-- Péndulo -->
              <th>Áng Pénd°</th>
              <th>Vel Ang°/s</th>
              <!-- PID Ángulo -->
              <th class="pida-col">Setpoint°</th>
              <th class="pida-col">Error°</th>
              <th class="pida-col">Integral</th>
              <th class="pida-col">Derivada</th>
              <th class="pida-col">Output</th>
              <th class="pida-col">Out Sat</th>
              <th class="pida-col">Kp</th>
              <th class="pida-col">Ki</th>
              <th class="pida-col">Kd</th>
              <th class="pida-col">R.Trabajo°</th>
              <th class="pida-col">Tolerancia°</th>
              <th class="pida-col">PWM Min</th>
              <th class="pida-col">PWM Max</th>
              <th class="pida-col">Pos SP cm</th>
              <!-- PID Posición interno -->
              <th class="pida-col">PosErr</th>
              <th class="pida-col">PosInt</th>
              <th class="pida-col">PosDer</th>
              <th class="pida-col">PosOut</th>
              <!-- MPU -->
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
              <!-- Posición -->
              <td class="mono">{{ fmt(r.pos_ir_cm, 1) }}</td>
              <td class="mono">{{ fmt(r.pos_mag_cm, 1) }}</td>
              <td class="mono">{{ fmt(r.rpm_ir, 0) }}</td>
              <td class="mono">{{ fmt(r.rpm_mag, 0) }}</td>
              <!-- Péndulo -->
              <td class="mono">{{ fmt(r.angle_pend, 1) }}</td>
              <td class="mono">{{ fmt(r.vel_ang_pend, 1) }}</td>
              <!-- PID Ángulo -->
              <td class="mono pida-val">{{ fmt(r.pida_setpoint, 1) }}</td>
              <td class="mono pida-val" :class="errorClass(r.pida_error)">{{ fmt(r.pida_error, 2) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_integral, 1) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_derivada, 2) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_output, 0) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_output_sat, 0) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_kp, 2) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_ki, 3) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_kd, 3) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_rango_trabajo, 1) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_tolerancia, 1) }}</td>
              <td class="mono pida-val">{{ r.pida_pwm_min ?? 0 }}</td>
              <td class="mono pida-val">{{ r.pida_pwm_max ?? 0 }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_pos_setpoint, 1) }}</td>
              <!-- PID Posición interno -->
              <td class="mono pida-val">{{ fmt(r.pida_pos_error, 2) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_pos_integral, 1) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_pos_derivada, 2) }}</td>
              <td class="mono pida-val">{{ fmt(r.pida_pos_output, 0) }}</td>
              <!-- MPU -->
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
          <p class="sin-datos-sub">Los datos se capturan durante la ejecución del control PID de ángulo.</p>
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

const registros = computed(() => store.pidAngResults)

// Filas visibles según selector
const registrosVisibles = computed(() => {
  const arr = registros.value
  const max = maxFilasVisibles.value
  return arr.length > max ? arr.slice(arr.length - max) : arr
})

const DIR_MAP = { 1: 'ADL', '-1': 'ATR', 0: 'DET', 'ADELANTE': 'ADL', 'ATRAS': 'ATR', 'DETENIDO': 'DET' }
function dirTexto(d) { return DIR_MAP[d] ?? (typeof d === 'string' ? d.slice(0, 3).toUpperCase() : 'DET') }

function fmt(val, decimals) {
  return (val ?? 0).toFixed(decimals)
}

// Clase para colorear el error según magnitud
function errorClass(error) {
  const absErr = Math.abs(error ?? 0)
  if (absErr < 2) return 'error-ok'
  if (absErr < 6) return 'error-near'
  return 'error-far'
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
    'Pos_IR_cm', 'Pos_Mag_cm', 'RPM_IR', 'RPM_Mag',
    'Angle_Pend_deg', 'Vel_Ang_Pend_deg_s',
    // PID Ángulo
    'PIDA_Setpoint', 'PIDA_Error', 'PIDA_Integral', 'PIDA_Derivada',
    'PIDA_Output', 'PIDA_Output_Sat',
    'PIDA_Kp', 'PIDA_Ki', 'PIDA_Kd',
    'PIDA_Rango_Trabajo', 'PIDA_Tolerancia',
    'PIDA_PWM_Min', 'PIDA_PWM_Max', 'PIDA_Pos_Setpoint',
    // PID Posición interno
    'PIDA_Pos_Error', 'PIDA_Pos_Integral', 'PIDA_Pos_Derivada', 'PIDA_Pos_Output',
    // MPU
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
    // Posición
    fmt(r.pos_ir_cm, 3),
    fmt(r.pos_mag_cm, 3),
    fmt(r.rpm_ir, 2),
    fmt(r.rpm_mag, 2),
    // Péndulo
    fmt(r.angle_pend, 3),
    fmt(r.vel_ang_pend, 3),
    // PID Ángulo
    fmt(r.pida_setpoint, 3),
    fmt(r.pida_error, 4),
    fmt(r.pida_integral, 3),
    fmt(r.pida_derivada, 4),
    fmt(r.pida_output, 2),
    fmt(r.pida_output_sat, 2),
    fmt(r.pida_kp, 4),
    fmt(r.pida_ki, 4),
    fmt(r.pida_kd, 4),
    fmt(r.pida_rango_trabajo, 2),
    fmt(r.pida_tolerancia, 2),
    r.pida_pwm_min ?? 0,
    r.pida_pwm_max ?? 0,
    fmt(r.pida_pos_setpoint, 2),
    // PID Posición interno
    fmt(r.pida_pos_error, 3),
    fmt(r.pida_pos_integral, 3),
    fmt(r.pida_pos_derivada, 3),
    fmt(r.pida_pos_output, 1),
    // MPU
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
  a.download = `pid_angulo_${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.csv`
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
  background: #E1BEE7;
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
  font-weight: 600;
  color: #666;
  white-space: nowrap;
}

.input-fecha {
  font-size: 0.7rem;
  padding: 3px 6px;
  border: 1px solid var(--md-outline);
  border-radius: 4px;
  background: var(--md-surface);
  color: var(--md-on-surface);
}

.rango-sep {
  color: #999;
}

.btn-clear-rango {
  border: none;
  background: #FFEBEE;
  color: #C62828;
  padding: 2px 6px;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.72rem;
}

/* ── Tabla ────────────────────────────────── */
.tabla-wrapper {
  overflow-x: auto;
  max-height: 400px;
  overflow-y: auto;
}

.tabla-resultados {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.68rem;
  white-space: nowrap;
}

.tabla-resultados th {
  position: sticky;
  top: 0;
  background: #F3E5F5;
  padding: 6px 8px;
  font-weight: 700;
  text-align: left;
  color: #4A148C;
  border-bottom: 2px solid #CE93D8;
  z-index: 1;
}

.tabla-resultados td {
  padding: 4px 8px;
  border-bottom: 1px solid #F5F5F5;
}

.tabla-resultados tbody tr:hover {
  background: #FFF8E1;
}

.mono {
  font-family: 'JetBrains Mono', monospace;
}

.pida-col {
  background: #EDE7F6 !important;
}

.pida-val {
  color: #4A148C;
}

.ls-cell {
  text-align: center;
}

.estado-cell {
  font-size: 0.62rem;
  font-weight: 600;
}

.error-ok { color: #2E7D32; }
.error-near { color: #F57F17; }
.error-far { color: #C62828; }

.sin-datos {
  text-align: center;
  padding: 32px;
  color: #999;
}

.sin-datos-sub {
  font-size: 0.72rem;
  color: #BBB;
  margin-top: 4px;
}
</style>
