<template>
  <div class="card">
    <div class="card-header">
      <span class="card-title">Resultados PID Cascada</span>
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
        <button class="btn btn-limpiar" @click="store.limpiarResultadosPidCasc()" :disabled="!registros.length">
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
              <!-- Lazo Externo (Posición) -->
              <th class="pidc-pos-col">Pos SP cm</th>
              <th class="pidc-pos-col">Pos Err cm</th>
              <th class="pidc-pos-col">Pos Int</th>
              <th class="pidc-pos-col">Pos Der</th>
              <th class="pidc-pos-col">Pos Out</th>
              <th class="pidc-pos-col">θ_ref°</th>
              <th class="pidc-pos-col">Kp Pos</th>
              <th class="pidc-pos-col">Ki Pos</th>
              <th class="pidc-pos-col">Kd Pos</th>
              <th class="pidc-pos-col">Zona Seg</th>
              <!-- Lazo Interno (Ángulo) -->
              <th class="pidc-ang-col">Áng SP°</th>
              <th class="pidc-ang-col">Áng Err°</th>
              <th class="pidc-ang-col">Áng Int</th>
              <th class="pidc-ang-col">Áng Der</th>
              <th class="pidc-ang-col">Áng Out</th>
              <th class="pidc-ang-col">Áng Out Sat</th>
              <th class="pidc-ang-col">Kp Áng</th>
              <th class="pidc-ang-col">Ki Áng</th>
              <th class="pidc-ang-col">Kd Áng</th>
              <!-- Parámetros comunes -->
              <th>R.Trabajo°</th>
              <th>Tol°</th>
              <th>PWM Min</th>
              <th>PWM Max</th>
              <th>θ_ref max</th>
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
              <!-- Lazo Externo (Posición) -->
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_setpoint, 1) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_error, 2) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_integral, 1) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_derivada, 2) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_output, 2) }}</td>
              <td class="mono pidc-pos-val" :class="thetaRefClass(r.pidc_pos_output_sat)">{{ fmt(r.pidc_pos_output_sat, 3) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_kp, 3) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_ki, 4) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_kd, 3) }}</td>
              <td class="mono pidc-pos-val">{{ fmt(r.pidc_pos_zona_seg, 1) }}</td>
              <!-- Lazo Interno (Ángulo) -->
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_setpoint, 3) }}</td>
              <td class="mono pidc-ang-val" :class="errorClass(r.pidc_ang_error)">{{ fmt(r.pidc_ang_error, 2) }}</td>
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_integral, 1) }}</td>
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_derivada, 2) }}</td>
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_output, 0) }}</td>
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_output_sat, 0) }}</td>
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_kp, 2) }}</td>
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_ki, 3) }}</td>
              <td class="mono pidc-ang-val">{{ fmt(r.pidc_ang_kd, 3) }}</td>
              <!-- Parámetros comunes -->
              <td class="mono">{{ fmt(r.pidc_rango_trabajo, 1) }}</td>
              <td class="mono">{{ fmt(r.pidc_tolerancia_ang, 1) }}</td>
              <td class="mono">{{ r.pidc_pwm_min ?? 0 }}</td>
              <td class="mono">{{ r.pidc_pwm_max ?? 0 }}</td>
              <td class="mono">{{ fmt(r.pidc_theta_ref_max, 2) }}</td>
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
          <p class="sin-datos-sub">Los datos se capturan durante la ejecución del control PID en cascada.</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const maxFilasVisibles = ref(500)
const rangoDesde = ref('')
const rangoHasta = ref('')

const registros = computed(() => store.pidCascResults)

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

function errorClass(error) {
  const absErr = Math.abs(error ?? 0)
  if (absErr < 2) return 'error-ok'
  if (absErr < 6) return 'error-near'
  return 'error-far'
}

function thetaRefClass(thetaRef) {
  const absRef = Math.abs(thetaRef ?? 0)
  if (absRef < 1) return 'error-ok'
  if (absRef < 1.8) return 'error-near'
  return 'error-far'
}

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
    // Lazo Externo (Posición)
    'PIDC_Pos_Setpoint', 'PIDC_Pos_Error', 'PIDC_Pos_Integral', 'PIDC_Pos_Derivada',
    'PIDC_Pos_Output', 'PIDC_Pos_Output_Sat_ThetaRef',
    'PIDC_Pos_Kp', 'PIDC_Pos_Ki', 'PIDC_Pos_Kd', 'PIDC_Pos_Zona_Seg',
    // Lazo Interno (Ángulo)
    'PIDC_Ang_Setpoint', 'PIDC_Ang_Error', 'PIDC_Ang_Integral', 'PIDC_Ang_Derivada',
    'PIDC_Ang_Output', 'PIDC_Ang_Output_Sat',
    'PIDC_Ang_Kp', 'PIDC_Ang_Ki', 'PIDC_Ang_Kd',
    // Parámetros comunes
    'PIDC_Rango_Trabajo', 'PIDC_Tolerancia_Ang',
    'PIDC_PWM_Min', 'PIDC_PWM_Max', 'PIDC_Theta_Ref_Max',
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
    fmt(r.pos_ir_cm, 3),
    fmt(r.pos_mag_cm, 3),
    fmt(r.rpm_ir, 2),
    fmt(r.rpm_mag, 2),
    fmt(r.angle_pend, 3),
    fmt(r.vel_ang_pend, 3),
    // Lazo Externo
    fmt(r.pidc_pos_setpoint, 3),
    fmt(r.pidc_pos_error, 4),
    fmt(r.pidc_pos_integral, 3),
    fmt(r.pidc_pos_derivada, 4),
    fmt(r.pidc_pos_output, 3),
    fmt(r.pidc_pos_output_sat, 4),
    fmt(r.pidc_pos_kp, 4),
    fmt(r.pidc_pos_ki, 5),
    fmt(r.pidc_pos_kd, 4),
    fmt(r.pidc_pos_zona_seg, 2),
    // Lazo Interno
    fmt(r.pidc_ang_setpoint, 4),
    fmt(r.pidc_ang_error, 4),
    fmt(r.pidc_ang_integral, 3),
    fmt(r.pidc_ang_derivada, 4),
    fmt(r.pidc_ang_output, 2),
    fmt(r.pidc_ang_output_sat, 2),
    fmt(r.pidc_ang_kp, 4),
    fmt(r.pidc_ang_ki, 4),
    fmt(r.pidc_ang_kd, 4),
    // Comunes
    fmt(r.pidc_rango_trabajo, 2),
    fmt(r.pidc_tolerancia_ang, 2),
    r.pidc_pwm_min ?? 0,
    r.pidc_pwm_max ?? 0,
    fmt(r.pidc_theta_ref_max, 3),
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
  a.download = `pid_cascada_${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.csv`
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
  background: #B2DFDB;
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

/* ── Rango de exportación ── */
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

/* ── Tabla ── */
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
  background: #E0F2F1;
  padding: 6px 8px;
  font-weight: 700;
  text-align: left;
  color: #004D40;
  border-bottom: 2px solid #80CBC4;
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

.pidc-pos-col {
  background: #E0F2F1 !important;
}

.pidc-ang-col {
  background: #EDE7F6 !important;
}

.pidc-pos-val {
  color: #004D40;
}

.pidc-ang-val {
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
