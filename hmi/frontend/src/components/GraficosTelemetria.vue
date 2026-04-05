<template>
  <div class="card">
    <div class="card-header graficos-header">
      <!-- Título y selector de ventana temporal -->
      <div class="header-izq">
        <span class="header-titulo">Gráficos de Telemetría</span>
        <div class="selector-ventana">
          <span class="selector-icono">⏱</span>
          <button
            v-for="opt in opcionesVentana"
            :key="opt.valor"
            class="btn-ventana"
            :class="{ activo: ventanaSeleccionada === opt.valor }"
            @click="ventanaSeleccionada = opt.valor"
          >{{ opt.label }}</button>
        </div>
      </div>
      <!-- Toggles de gráficos agrupados -->
      <div class="header-der">
        <button
          v-for="g in graficos"
          :key="g.id"
          class="btn-toggle"
          :class="{ activo: g.visible }"
          @click="g.visible = !g.visible"
        >{{ g.label }}</button>
      </div>
    </div>
    <div class="card-body graficos-container" :class="{ 'layout-grid': layout === 'grid' }">
      <div
        v-for="g in graficosVisibles"
        :key="g.id"
        class="grafico-wrapper"
      >
        <div class="grafico-header">
          <span class="grafico-label">{{ g.label }} <span class="text-muted text-xs">({{ g.unit }})</span></span>
          <!-- Leyenda para gráficos multi-serie -->
          <div v-if="g.series.length > 1" class="grafico-leyenda">
            <span
              v-for="s in g.series"
              :key="s.key"
              class="leyenda-item"
            >
              <span class="leyenda-color" :style="{ background: s.color }"></span>
              {{ s.label }}
            </span>
          </div>
        </div>
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
  layout: { type: String, default: 'column' }, // 'column' o 'grid'
})

const layout = computed(() => props.layout)

// ── Opciones de ventana temporal ─────────────
const opcionesVentana = [
  { valor: 10, label: '10s' },
  { valor: 30, label: '30s' },
  { valor: 60, label: '60s' },
  { valor: 0, label: '∞' },
]

// ── Definición de gráficos agrupados ─────────
// Cada gráfico puede tener múltiples series (variables del mismo tipo)
const graficos = reactive([
  {
    id: 'angulo',
    label: 'Ángulo',
    unit: '°',
    visible: props.graficosDefault ? props.graficosDefault.includes('angulo') : true,
    min: -20,
    max: 380,
    series: [
      { key: 'angle_pend', label: 'Péndulo', color: '#8C2981' },
    ],
  },
  {
    id: 'vel_angular',
    label: 'Vel. Angular',
    unit: '°/s',
    visible: props.graficosDefault ? props.graficosDefault.includes('vel_angular') : false,
    min: -500,
    max: 500,
    series: [
      { key: 'vel_ang_pend', label: 'Péndulo', color: '#00ACC1' },
    ],
  },
  {
    id: 'posicion',
    label: 'Posición',
    unit: 'cm',
    visible: props.graficosDefault ? props.graficosDefault.includes('posicion') : true,
    min: -5,
    max: 100,
    series: [
      { key: 'pos_ir_cm', label: 'IR', color: '#1E88E5' },
      { key: 'pos_mag_cm', label: 'Mag', color: '#43A047' },
      { key: 'pid_setpoint', label: 'Setpoint', color: '#E91E63' },
    ],
  },
  {
    id: 'rpm',
    label: 'RPM',
    unit: 'rpm',
    visible: props.graficosDefault ? props.graficosDefault.includes('rpm') : false,
    min: -5000,
    max: 5000,
    series: [
      { key: 'rpm_ir', label: 'IR', color: '#E53935' },
      { key: 'rpm_mag', label: 'Mag', color: '#00897B' },
    ],
  },
  {
    id: 'aceleracion',
    label: 'Aceleración',
    unit: 'm/s²',
    visible: props.graficosDefault ? props.graficosDefault.includes('aceleracion') : false,
    min: -15,
    max: 15,
    series: [
      { key: 'mpu_ax', label: 'AX', color: '#E53935' },
      { key: 'mpu_ay', label: 'AY', color: '#43A047' },
      { key: 'mpu_az', label: 'AZ', color: '#1E88E5' },
    ],
  },
  {
    id: 'rotacion',
    label: 'Rotación',
    unit: '°',
    visible: props.graficosDefault ? props.graficosDefault.includes('rotacion') : false,
    min: -90,
    max: 90,
    series: [
      { key: 'mpu_pitch', label: 'Pitch', color: '#AB47BC' },
      { key: 'mpu_roll', label: 'Roll', color: '#26C6DA' },
    ],
  },
  {
    id: 'pwm',
    label: 'PWM',
    unit: '0-1023',
    visible: props.graficosDefault ? props.graficosDefault.includes('pwm') : true,
    min: 0,
    max: 1023,
    series: [
      { key: 'pwm', label: 'PWM', color: '#FC8961' },
    ],
  },
  // ── Gráficos PID ─────────────────────────────
  {
    id: 'pid_output',
    label: 'Salida PID',
    unit: '',
    visible: props.graficosDefault ? props.graficosDefault.includes('pid_output') : false,
    min: null,
    max: null,
    series: [
      { key: 'pid_output', label: 'Output', color: '#1565C0' },
      { key: 'pid_output_sat', label: 'Output Sat', color: '#FF6D00' },
    ],
  },
  {
    id: 'pid_error',
    label: 'Error PID',
    unit: 'cm',
    visible: props.graficosDefault ? props.graficosDefault.includes('pid_error') : false,
    min: null,
    max: null,
    series: [
      { key: 'pid_error', label: 'Error', color: '#D32F2F' },
    ],
  },
  {
    id: 'pid_integral',
    label: 'Integral PID',
    unit: '',
    visible: props.graficosDefault ? props.graficosDefault.includes('pid_integral') : false,
    min: null,
    max: null,
    series: [
      { key: 'pid_integral', label: 'Integral', color: '#7B1FA2' },
    ],
  },
  {
    id: 'pid_derivada',
    label: 'Derivada PID',
    unit: '',
    visible: props.graficosDefault ? props.graficosDefault.includes('pid_derivada') : false,
    min: null,
    max: null,
    series: [
      { key: 'pid_derivada', label: 'Derivada', color: '#1976D2' },
    ],
  },
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
  const h = 160  // Altura aumentada

  // Serie 0 = eje X (tiempo)
  const series = [{}]

  // Agregar cada serie de datos del grupo
  for (const s of grafico.series) {
    series.push({
      label: s.label,
      stroke: s.color,
      width: 1.5,
      fill: s.color + '15',
    })
  }

  const opts = {
    width: w,
    height: h,
    cursor: { show: true, sync: { key: 'telemetria' } },
    legend: { show: false },  // Usamos leyenda custom en el template
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
        range: (u, dataMin, dataMax) => {
          // Rango dinámico: usa preset como base, expande si los datos exceden
          const pad = (dataMax - dataMin) * 0.05 || 1
          const lo = grafico.min != null ? Math.min(grafico.min, dataMin - pad) : dataMin - pad
          const hi = grafico.max != null ? Math.max(grafico.max, dataMax + pad) : dataMax + pad
          return [lo, hi]
        },
      },
    },
    series,
  }

  // Datos iniciales vacíos: [timestamps, serie1, serie2, ...]
  const data = [[], ...grafico.series.map(() => [])]

  plotMap[grafico.id] = new uPlot(opts, data, el)
  observarCanvas(grafico.id, el)
}

function destruirPlot(id) {
  desobservarCanvas(id)
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

    // Construir array de datos: [timestamps, serie1, serie2, ...]
    const data = [tsSegs]

    for (const s of grafico.series) {
      const valores = h[s.key]
      if (valores) {
        data.push(valores.slice(startIdx))
      } else {
        data.push([])
      }
    }

    plot.setData(data)
  }
}

// ── Lifecycle ────────────────────────────────
let resizeObserver = null
let resizeTimer = null

function manejarResize(entries) {
  if (resizeTimer) clearTimeout(resizeTimer)
  resizeTimer = setTimeout(() => {
    for (const entry of entries) {
      const el = entry.target
      const id = el.dataset.plotId
      if (id && plotMap[id]) {
        const w = el.clientWidth || 600
        plotMap[id].setSize({ width: w, height: 160 })
      }
    }
  }, 150)
}

function observarCanvas(id, el) {
  if (!resizeObserver || !el) return
  el.dataset.plotId = id
  resizeObserver.observe(el)
}

function desobservarCanvas(id) {
  const el = refMap[id]
  if (resizeObserver && el) {
    resizeObserver.unobserve(el)
  }
}

onMounted(() => {
  resizeObserver = new ResizeObserver(manejarResize)
  // Timer de actualización: ~10 Hz (suficiente para gráficos)
  updateTimer = setInterval(actualizarGraficos, 100)
})

onUnmounted(() => {
  if (updateTimer) clearInterval(updateTimer)
  if (resizeTimer) clearTimeout(resizeTimer)
  if (resizeObserver) {
    resizeObserver.disconnect()
    resizeObserver = null
  }
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
/* ── Header reorganizado ────────────────────── */
.graficos-header {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
}

.header-izq {
  display: flex;
  align-items: center;
  gap: 16px;
  flex-wrap: wrap;
}

.header-titulo {
  font-weight: 600;
  font-size: 0.9rem;
}

.selector-ventana {
  display: flex;
  align-items: center;
  gap: 2px;
  background: var(--md-surface-variant, #f5f5f5);
  border-radius: 8px;
  padding: 2px;
}

.selector-icono {
  padding: 0 6px;
  font-size: 0.8rem;
  opacity: 0.7;
}

.btn-ventana {
  padding: 4px 10px;
  border: none;
  border-radius: 6px;
  background: transparent;
  font-size: 0.7rem;
  font-weight: 500;
  cursor: pointer;
  color: var(--md-on-surface-secondary);
  transition: all var(--transition-fast, 0.15s);
}

.btn-ventana.activo {
  background: var(--md-primary);
  color: var(--md-on-primary, white);
  box-shadow: 0 1px 3px rgba(0,0,0,0.15);
}

.btn-ventana:hover:not(.activo) {
  background: rgba(0,0,0,0.05);
}

.header-der {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
}

/* ── Toggles de gráficos ────────────────────── */
.btn-toggle {
  padding: 4px 10px;
  border: 1px solid var(--md-outline);
  border-radius: 12px;
  background: transparent;
  font-size: 0.7rem;
  cursor: pointer;
  color: var(--md-on-surface-secondary);
  transition: all var(--transition-fast, 0.15s);
}

.btn-toggle.activo {
  background: var(--md-primary);
  color: var(--md-on-primary, white);
  border-color: var(--md-primary);
}

.btn-toggle:hover:not(.activo) {
  border-color: var(--md-primary);
  background: rgba(var(--md-primary-rgb, 103, 80, 164), 0.08);
}

/* ── Container de gráficos ──────────────────── */
.graficos-container {
  display: flex;
  flex-direction: column;
  gap: 12px;
  min-width: 0;
}

/* ── Layout Grid 2x2 ── */
.graficos-container.layout-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 16px;
  min-width: 0;
}

.layout-grid .grafico-wrapper {
  border-bottom: none;
  padding-bottom: 0;
  border: 1px solid var(--md-outline-variant, #E0E0E0);
  border-radius: 8px;
  padding: 12px;
  background: var(--md-surface, #FAFAFA);
}

.layout-grid .grafico-wrapper {
  min-width: 0;
  overflow: hidden;
}

.layout-grid .grafico-canvas {
  min-height: 140px;
}

.grafico-wrapper {
  border-bottom: 1px solid var(--md-outline);
  padding-bottom: 12px;
  min-width: 0;
  overflow: hidden;
}

.grafico-wrapper:last-child {
  border-bottom: none;
  padding-bottom: 0;
}

/* Responsive grid */
@media (max-width: 800px) {
  .graficos-container.layout-grid {
    grid-template-columns: 1fr;
  }
}

.grafico-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 4px;
  padding: 0 4px;
}

.grafico-label {
  font-size: 0.8rem;
  font-weight: 500;
  color: var(--md-on-surface-secondary);
}

/* ── Leyenda para gráficos multi-serie ──────── */
.grafico-leyenda {
  display: flex;
  gap: 10px;
}

.leyenda-item {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 0.65rem;
  color: var(--md-on-surface-secondary);
}

.leyenda-color {
  width: 10px;
  height: 3px;
  border-radius: 2px;
}

/* ── Canvas del gráfico ─────────────────────── */
.grafico-canvas {
  width: 100%;
  min-width: 0;
  min-height: 160px;
  overflow: hidden;
}
</style>
