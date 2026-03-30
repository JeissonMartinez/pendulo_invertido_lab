<template>
  <div class="card">
    <div class="card-header flex-row justify-between items-center">
      <span>Gráficos de Telemetría</span>
      <div class="flex-row gap-sm flex-wrap items-center">
        <select v-model="ventanaSeleccionada" class="select-ventana">
          <option :value="0">Todo</option>
          <option :value="10">10s</option>
          <option :value="30">30s</option>
          <option :value="60">60s</option>
        </select>
        <button
          v-for="g in graficos"
          :key="g.id"
          class="btn-toggle"
          :class="{ activo: g.visible }"
          @click="g.visible = !g.visible"
        >{{ g.label }}</button>
      </div>
    </div>
    <div class="card-body graficos-container">
      <div
        v-for="g in graficosVisibles"
        :key="g.id"
        class="grafico-wrapper"
      >
        <div class="grafico-label">{{ g.label }} <span class="text-muted text-xs">({{ g.unit }})</span></div>
        <div :ref="el => setRef(g.id, el)" class="grafico-canvas"></div>
      </div>
      <div v-if="graficosVisibles.length === 0" class="text-center text-muted" style="padding: 40px">
        Selecciona al menos un gráfico para visualizar
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, watch, onMounted, onUnmounted, nextTick } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'
import uPlot from 'uplot'
import 'uplot/dist/uPlot.min.css'

const store = usePendulumStore()

const props = defineProps({
  graficosDefault: { type: Array, default: null },
  ventanaTiempo: { type: Number, default: 0 }, // 0 = todo, en segundos
})

// ── Definición de gráficos ───────────────────
const graficos = reactive([
  { id: 'angle',       label: 'Ángulo',         unit: '°',     color: '#8C2981', visible: props.graficosDefault ? props.graficosDefault.includes('angle')       : true,  min: null, max: null, setpoint: 0, dataKey: 'angle_pend_unwrap' },
  { id: 'position',    label: 'Posición',       unit: 'cm',    color: '#B5367A', visible: props.graficosDefault ? props.graficosDefault.includes('position')    : true,  min: 0,   max: 95,  setpoint: 45.0 },
  { id: 'pwm',         label: 'PWM',            unit: '0-1023',color: '#FC8961', visible: props.graficosDefault ? props.graficosDefault.includes('pwm')         : true,  min: 0,   max: 1023 },
  { id: 'cart_accel',  label: 'Aceleración',    unit: 'm/s²',  color: '#51127C', visible: props.graficosDefault ? props.graficosDefault.includes('cart_accel')  : false, min: -5,  max: 5 },
  { id: 'rpm',         label: 'RPM',            unit: 'rpm',   color: '#DE4968', visible: props.graficosDefault ? props.graficosDefault.includes('rpm')         : false, min: 0,   max: 600 },
  // Series extendidas
  { id: 'rpm_ir',       label: 'RPM IR',         unit: 'rpm',   color: '#E53935', visible: props.graficosDefault ? props.graficosDefault.includes('rpm_ir')      : false, min: 0,   max: 600 },
  { id: 'rps_ir',       label: 'RPS IR',         unit: 'rev/s', color: '#D81B60', visible: props.graficosDefault ? props.graficosDefault.includes('rps_ir')      : false, min: 0,   max: 15 },
  { id: 'pos_ir_cm',    label: 'Pos IR',         unit: 'cm',    color: '#8E24AA', visible: props.graficosDefault ? props.graficosDefault.includes('pos_ir_cm')   : false, min: 0,   max: 95 },
  { id: 'rpm_mag',      label: 'RPM Mag',        unit: 'rpm',   color: '#5E35B1', visible: props.graficosDefault ? props.graficosDefault.includes('rpm_mag')     : false, min: 0,   max: 600 },
  { id: 'pos_mag_cm',   label: 'Pos Mag',        unit: 'cm',    color: '#3949AB', visible: props.graficosDefault ? props.graficosDefault.includes('pos_mag_cm')  : false, min: 0,   max: 95 },
  { id: 'angle_pend',   label: 'Ang. Pénd.',     unit: '°',     color: '#1E88E5', visible: props.graficosDefault ? props.graficosDefault.includes('angle_pend')  : false, min: null, max: null, setpoint: 0, dataKey: 'angle_pend_unwrap' },
  { id: 'vel_ang_pend', label: 'Vel. Ang.',      unit: '°/s',   color: '#00ACC1', visible: props.graficosDefault ? props.graficosDefault.includes('vel_ang_pend'): false, min: -500, max: 500 },
  { id: 'mpu_ax',       label: 'MPU Ax',         unit: 'm/s²',  color: '#00897B', visible: props.graficosDefault ? props.graficosDefault.includes('mpu_ax')     : false, min: -15, max: 15 },
  { id: 'mpu_ay',       label: 'MPU Ay',         unit: 'm/s²',  color: '#43A047', visible: props.graficosDefault ? props.graficosDefault.includes('mpu_ay')     : false, min: -15, max: 15 },
  { id: 'mpu_az',       label: 'MPU Az',         unit: 'm/s²',  color: '#7CB342', visible: props.graficosDefault ? props.graficosDefault.includes('mpu_az')     : false, min: -15, max: 15 },
  { id: 'mpu_pitch',    label: 'MPU Pitch',      unit: '°',     color: '#C0CA33', visible: props.graficosDefault ? props.graficosDefault.includes('mpu_pitch')  : false, min: -90, max: 90 },
  { id: 'mpu_roll',     label: 'MPU Roll',       unit: '°',     color: '#FDD835', visible: props.graficosDefault ? props.graficosDefault.includes('mpu_roll')   : false, min: -90, max: 90 },
])

const graficosVisibles = computed(() => graficos.filter(g => g.visible))

const ventanaSeleccionada = ref(props.ventanaTiempo || 0)

// ── Referencias DOM ──────────────────────────
const refMap = {}
function setRef(id, el) {
  refMap[id] = el
}

// ── Instancias uPlot ─────────────────────────
const plotMap = {}

function crearPlot(grafico, el) {
  if (!el || plotMap[grafico.id]) return

  const w = el.clientWidth || 600
  const h = 120

  const series = [
    {},  // eje X (tiempo)
    {
      label: grafico.label,
      stroke: grafico.color,
      width: 1.5,
      fill: grafico.color + '15',
    },
  ]

  // Agregar serie de setpoint si existe
  if (grafico.setpoint !== undefined) {
    series.push({
      label: 'Setpoint',
      stroke: '#FC8961',
      width: 1,
      dash: [4, 3],
    })
  }

  const opts = {
    width: w,
    height: h,
    cursor: { show: false },
    legend: { show: false },
    axes: [
      {
        stroke: '#9E9E9E',
        grid: { stroke: '#F5F5F5' },
        ticks: { show: false },
        values: (u, vals) => vals.map(v => {
          const s = Math.round((Date.now() / 1000 - v))
          return `-${s}s`
        }),
      },
      {
        stroke: '#9E9E9E',
        grid: { stroke: '#F0F0F0', dash: [2, 2] },
        size: 50,
      },
    ],
    scales: {
      y: {
        min: grafico.min,
        max: grafico.max,
        auto: grafico.min === null,
      },
    },
    series,
  }

  // Datos iniciales vacíos
  const data = grafico.setpoint !== undefined
    ? [[], [], []]
    : [[], []]

  plotMap[grafico.id] = new uPlot(opts, data, el)
}

function destruirPlot(id) {
  if (plotMap[id]) {
    plotMap[id].destroy()
    delete plotMap[id]
  }
}

// ── Actualización de datos ───────────────────
let updateTimer = null

function actualizarGraficos() {
  const h = store.historial
  if (h.ts.length === 0) return

  // Aplicar ventana de tiempo
  let startIdx = 0
  const ventana = ventanaSeleccionada.value
  if (ventana > 0) {
    const cutoff = Date.now() - ventana * 1000
    startIdx = h.ts.findIndex(t => t >= cutoff)
    if (startIdx < 0) startIdx = h.ts.length
  }

  // Convertir timestamps a segundos (solo ventana visible)
  const tsSegs = h.ts.slice(startIdx).map(t => t / 1000)

  for (const grafico of graficos) {
    const plot = plotMap[grafico.id]
    if (!plot) continue

    const valores = h[grafico.dataKey || grafico.id]
    if (!valores) continue

    const data = [tsSegs, valores.slice(startIdx)]

    // Agregar línea de setpoint si existe
    if (grafico.setpoint !== undefined) {
      data.push(new Array(tsSegs.length).fill(grafico.setpoint))
    }

    plot.setData(data)
  }
}

// ── Lifecycle ────────────────────────────────
onMounted(() => {
  // Timer de actualización: ~10 Hz (suficiente para gráficos)
  updateTimer = setInterval(actualizarGraficos, 100)
})

onUnmounted(() => {
  if (updateTimer) clearInterval(updateTimer)
  for (const id of Object.keys(plotMap)) {
    destruirPlot(id)
  }
})

// Crear/destruir plots cuando cambia visibilidad
watch(graficosVisibles, async () => {
  await nextTick()

  // Destruir plots que ya no son visibles
  for (const id of Object.keys(plotMap)) {
    if (!graficos.find(g => g.id === id && g.visible)) {
      destruirPlot(id)
    }
  }

  // Crear plots nuevos
  for (const g of graficosVisibles.value) {
    if (!plotMap[g.id] && refMap[g.id]) {
      crearPlot(g, refMap[g.id])
    }
  }
}, { deep: true })

// Inicializar plots al primer render
watch(() => store.historial.ts.length, async (len) => {
  if (len > 0) {
    await nextTick()
    for (const g of graficosVisibles.value) {
      if (!plotMap[g.id] && refMap[g.id]) {
        crearPlot(g, refMap[g.id])
      }
    }
  }
}, { once: true })

// Crear plots vacíos inmediatamente al montar
onMounted(async () => {
  await nextTick()
  for (const g of graficosVisibles.value) {
    if (!plotMap[g.id] && refMap[g.id]) {
      crearPlot(g, refMap[g.id])
    }
  }
})
</script>

<style scoped>
.graficos-container {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.grafico-wrapper {
  border-bottom: 1px solid var(--md-outline);
  padding-bottom: 8px;
}

.grafico-wrapper:last-child {
  border-bottom: none;
  padding-bottom: 0;
}

.grafico-label {
  font-size: 0.75rem;
  font-weight: 500;
  color: var(--md-on-surface-secondary);
  margin-bottom: 2px;
  padding-left: 4px;
}

.grafico-canvas {
  width: 100%;
  overflow: hidden;
}

.btn-toggle {
  padding: 2px 8px;
  border: 1px solid var(--md-outline);
  border-radius: 12px;
  background: transparent;
  font-size: 0.65rem;
  cursor: pointer;
  color: var(--md-on-surface-secondary);
  transition: all var(--transition-fast);
}

.btn-toggle.activo {
  background: var(--md-primary);
  color: var(--md-on-primary);
  border-color: var(--md-primary);
}

.btn-toggle:hover {
  border-color: var(--md-primary);
}

.select-ventana {
  padding: 2px 6px;
  border: 1px solid var(--md-outline);
  border-radius: 8px;
  font-size: 0.65rem;
  background: white;
  cursor: pointer;
  color: var(--md-on-surface-secondary);
}

.select-ventana:focus {
  outline: none;
  border-color: var(--md-primary);
}
</style>
