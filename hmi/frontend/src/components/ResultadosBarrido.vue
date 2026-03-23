<template>
  <div class="card">
    <div class="card-header flex-row justify-between items-center">
      <span>Resultados del Barrido</span>
      <button class="btn-exportar" @click="exportarCSV">📥 Exportar CSV</button>
    </div>
    <div class="card-body">

      <!-- Resumen -->
      <div class="resumen-grid" v-if="datos">
        <div class="resumen-item resumen-min">
          <span class="resumen-label">PWM Mínimo</span>
          <span class="resumen-valor">{{ datos.pwmMin >= 0 ? datos.pwmMin : '—' }}</span>
          <span class="resumen-sub" v-if="datos.pwmMin >= 0">
            {{ (datos.pwmMin / 1023 * 100).toFixed(1) }}% del rango
          </span>
        </div>
        <div class="resumen-item resumen-max">
          <span class="resumen-label">PWM Máximo</span>
          <span class="resumen-valor">{{ datos.pwmMax >= 0 ? datos.pwmMax : '—' }}</span>
          <span class="resumen-sub" v-if="datos.pwmMax >= 0">
            {{ (datos.pwmMax / 1023 * 100).toFixed(1) }}% del rango
          </span>
        </div>
        <div class="resumen-item resumen-rango" v-if="datos.pwmMin >= 0 && datos.pwmMax >= 0">
          <span class="resumen-label">Rango Útil</span>
          <span class="resumen-valor">{{ datos.pwmMax - datos.pwmMin + 1 }}</span>
          <span class="resumen-sub">valores PWM</span>
        </div>
      </div>

      <!-- Tabla de registros -->
      <div class="tabla-wrapper" v-if="registros.length > 0">
        <table class="tabla-resultados">
          <thead>
            <tr>
              <th>#</th>
              <th>PWM</th>
              <th>¿Mueve?</th>
              <th>T. Ida (s)</th>
              <th>T. Vuelta (s)</th>
              <th>Dist. Ida (cm)</th>
              <th>Dist. Vuelta (cm)</th>
              <th>P. Ida</th>
              <th>P. Vuelta</th>
              <th>RPM Ida</th>
              <th>RPM Vuelta</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(r, i) in registros" :key="i" :class="{ 'fila-completada': r.completed, 'fila-no-mueve': !r.moved }">
              <td>{{ i + 1 }}</td>
              <td class="mono">{{ r.pwm }}</td>
              <td>
                <span v-if="!r.moved" class="estado-no">NO</span>
                <span v-else-if="r.completed" class="estado-ok">OK</span>
                <span v-else class="estado-parcial">SI*</span>
              </td>
              <td class="mono">{{ r.completed ? (r.timeFwdMs / 1000).toFixed(3) : '—' }}</td>
              <td class="mono">{{ r.completed ? (r.timeBackMs / 1000).toFixed(3) : '—' }}</td>
              <td class="mono">{{ r.completed ? (r.pulsesFwd * CM_POR_PULSO).toFixed(2) : '—' }}</td>
              <td class="mono">{{ r.completed ? (r.pulsesBack * CM_POR_PULSO).toFixed(2) : '—' }}</td>
              <td class="mono">{{ r.completed ? r.pulsesFwd : '—' }}</td>
              <td class="mono">{{ r.completed ? r.pulsesBack : '—' }}</td>
              <td class="mono">{{ r.completed ? r.rpmFwd.toFixed(1) : '—' }}</td>
              <td class="mono">{{ r.completed ? r.rpmBack.toFixed(1) : '—' }}</td>
            </tr>
          </tbody>
        </table>
      </div>

      <div v-else class="sin-datos">
        Sin registros de barrido.
      </div>

    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const CM_POR_PULSO = 0.0504

const datos = computed(() => store.sweepResults)
const registros = computed(() => datos.value?.records ?? [])

function exportarCSV() {
  if (!registros.value.length) return

  const headers = ['#', 'PWM', 'Mueve', 'Completado', 'T_Ida_s', 'T_Vuelta_s', 'Dist_Ida_cm', 'Dist_Vuelta_cm', 'P_Ida', 'P_Vuelta', 'RPM_Ida', 'RPM_Vuelta']
  const rows = registros.value.map((r, i) => [
    i + 1,
    r.pwm,
    r.moved ? 'SI' : 'NO',
    r.completed ? 'SI' : 'NO',
    r.completed ? (r.timeFwdMs / 1000).toFixed(3) : '',
    r.completed ? (r.timeBackMs / 1000).toFixed(3) : '',
    r.completed ? (r.pulsesFwd * CM_POR_PULSO).toFixed(2) : '',
    r.completed ? (r.pulsesBack * CM_POR_PULSO).toFixed(2) : '',
    r.completed ? r.pulsesFwd : '',
    r.completed ? r.pulsesBack : '',
    r.completed ? r.rpmFwd.toFixed(1) : '',
    r.completed ? r.rpmBack.toFixed(1) : '',
  ])

  const csv = [headers.join(','), ...rows.map(r => r.join(','))].join('\n')
  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = `barrido_pwm_${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.csv`
  a.click()
  URL.revokeObjectURL(url)
}
</script>

<style scoped>
.resumen-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 12px;
  margin-bottom: 16px;
}

.resumen-item {
  padding: 14px;
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.resumen-min { background: #F3E8F9; }
.resumen-max { background: #FDECDA; }
.resumen-rango { background: #E8F5E9; }

.resumen-label {
  font-size: 0.72rem;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  font-weight: 600;
}

.resumen-valor {
  font-size: 1.6rem;
  font-weight: 700;
  color: #8C2981;
}

.resumen-max .resumen-valor { color: #C4501A; }
.resumen-rango .resumen-valor { color: #2E7D32; }

.resumen-sub {
  font-size: 0.72rem;
  color: var(--md-on-surface-secondary);
}

/* ── Tabla ───────────────────────────────────── */
.tabla-wrapper {
  max-height: 400px;
  overflow-y: auto;
  border: 1px solid var(--md-outline);
  border-radius: 8px;
}

.tabla-resultados {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.78rem;
}

.tabla-resultados th {
  position: sticky;
  top: 0;
  background: var(--md-surface-dim);
  padding: 8px 10px;
  text-align: left;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  font-size: 0.7rem;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  border-bottom: 2px solid var(--md-outline);
}

.tabla-resultados td {
  padding: 6px 10px;
  border-bottom: 1px solid #EDE8F0;
  color: var(--md-on-surface);
}

.tabla-resultados tr:hover {
  background: #F8F4FA;
}

.fila-no-mueve {
  opacity: 0.5;
}

.fila-completada td {
  color: var(--md-on-surface);
}

.mono {
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
  font-size: 0.75rem;
}

.estado-ok { color: #2E7D32; font-weight: 600; }
.estado-no { color: #9E9E9E; }
.estado-parcial { color: #C4501A; font-weight: 600; }

.sin-datos {
  text-align: center;
  padding: 40px;
  color: var(--md-on-surface-secondary);
  font-size: 0.88rem;
}

/* ── Exportar ───────────────────────────────── */
.btn-exportar {
  border: 1px solid var(--md-outline);
  background: white;
  padding: 4px 10px;
  border-radius: 6px;
  font-size: 0.72rem;
  font-weight: 600;
  cursor: pointer;
  color: #8C2981;
  transition: all 0.15s;
}

.btn-exportar:hover {
  background: #F3E8F9;
  border-color: var(--md-outline-variant);
}
</style>
