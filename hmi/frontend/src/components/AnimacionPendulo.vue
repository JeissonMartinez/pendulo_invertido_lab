<template>
  <div class="card">
    <div class="card-header">Animación del Péndulo</div>
    <div class="card-body" ref="contenedor">
      <svg
        :viewBox="`0 0 ${svgW} ${svgH}`"
        :width="svgW"
        :height="svgH"
        style="width: 100%; height: auto; display: block;"
      >
        <!-- Fondo -->
        <rect :width="svgW" :height="svgH" fill="#FAFAFA" rx="8" />

        <!-- Zonas de alerta (bordes) -->
        <rect
          :x="pistaX"
          :y="pistaY - 40"
          :width="zonaAlertaPx"
          height="80"
          fill="rgba(255, 152, 0, 0.1)"
          rx="4"
        />
        <rect
          :x="pistaX + pistaW - zonaAlertaPx"
          :y="pistaY - 40"
          :width="zonaAlertaPx"
          height="80"
          fill="rgba(255, 152, 0, 0.1)"
          rx="4"
        />

        <!-- Pista -->
        <rect
          :x="pistaX"
          :y="pistaY"
          :width="pistaW"
          height="4"
          fill="#9E9E9E"
          rx="2"
        />

        <!-- Marcas de posición -->
        <g v-for="marca in marcas" :key="marca.cm">
          <line
            :x1="marca.x"
            :y1="pistaY + 10"
            :x2="marca.x"
            :y2="pistaY + 20"
            stroke="#BDBDBD"
            stroke-width="1"
          />
          <text
            :x="marca.x"
            :y="pistaY + 34"
            text-anchor="middle"
            fill="#9E9E9E"
            font-size="10"
          >{{ marca.cm }}</text>
        </g>

        <!-- Limit Switch INICIO -->
        <rect
          :x="pistaX - 8"
          :y="pistaY - 20"
          width="8"
          height="44"
          :fill="store.estado.limits.start_switch ? '#D32F2F' : '#4CAF50'"
          rx="2"
        />

        <!-- Limit Switch FIN -->
        <rect
          :x="pistaX + pistaW"
          :y="pistaY - 20"
          width="8"
          height="44"
          :fill="store.estado.limits.end_switch ? '#D32F2F' : '#4CAF50'"
          rx="2"
        />

        <!-- Carro -->
        <rect
          :x="carroX - carroW / 2"
          :y="pistaY - carroH"
          :width="carroW"
          :height="carroH"
          fill="#51127C"
          rx="4"
          stroke="#2D0053"
          stroke-width="1.5"
        />

        <!-- Ruedas del carro -->
        <circle
          :cx="carroX - carroW / 4"
          :cy="pistaY + 2"
          r="5"
          fill="#424242"
        />
        <circle
          :cx="carroX + carroW / 4"
          :cy="pistaY + 2"
          r="5"
          fill="#424242"
        />

        <!-- Pivote del péndulo -->
        <circle
          :cx="carroX"
          :cy="pivoteY"
          r="4"
          fill="#2D0053"
        />

        <!-- Barra del péndulo -->
        <line
          :x1="carroX"
          :y1="pivoteY"
          :x2="penduloPuntaX"
          :y2="penduloPuntaY"
          stroke="#FC8961"
          stroke-width="4"
          stroke-linecap="round"
        />

        <!-- Masa del péndulo -->
        <circle
          :cx="penduloPuntaX"
          :cy="penduloPuntaY"
          r="10"
          fill="#FC8961"
          stroke="#C4501A"
          stroke-width="1.5"
        />

        <!-- Setpoint de posición (línea punteada) -->
        <line
          :x1="setpointX"
          :y1="pistaY - 55"
          :x2="setpointX"
          :y2="pistaY + 30"
          stroke="#FC8961"
          stroke-width="1"
          stroke-dasharray="4 3"
          opacity="0.6"
        />
        <text
          :x="setpointX"
          :y="pistaY - 60"
          text-anchor="middle"
          fill="#FC8961"
          font-size="9"
          opacity="0.8"
        >SP</text>

        <!-- Texto de ángulo actual -->
        <text
          :x="svgW - 16"
          :y="24"
          text-anchor="end"
          fill="#616161"
          font-size="13"
          font-weight="500"
        >{{ anguloDisplay }}°</text>

        <!-- Texto de posición actual -->
        <text
          :x="svgW - 16"
          :y="42"
          text-anchor="end"
          fill="#616161"
          font-size="11"
        >{{ posicionDisplay }} cm</text>
      </svg>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

// ── Dimensiones SVG ──────────────────────────
const svgW = 700
const svgH = 280
const pistaX = 50
const pistaW = 600
const pistaY = 140   // Y de la pista — centrada para dejar espacio abajo para el péndulo
const carroW = 50
const carroH = 24
const barraLen = 100  // longitud visual del péndulo (px)

// Pista real: 90 cm
const PISTA_CM = 90

const zonaAlertaCm = computed(() => {
  if (store.experimentoActivo === 'pid_posicion') {
    return store.pidPosState.zona_seguridad ?? 10
  }
  return 10
})

const setpointCm = computed(() => {
  if (store.experimentoActivo === 'pid_posicion') {
    return store.pidPosState.setpoint ?? 45
  }
  return 45
})

const zonaAlertaPx = computed(() => (zonaAlertaCm.value / PISTA_CM) * pistaW)
const setpointX = computed(() => pistaX + (setpointCm.value / PISTA_CM) * pistaW)

// ── Marcas de posición ───────────────────────
const marcas = [0, 10, 20, 30, 45, 60, 70, 80, 90].map(cm => ({
  cm,
  x: pistaX + (cm / PISTA_CM) * pistaW,
}))

// ── Interpolación suave ──────────────────────
const interpolAngle = ref(0)        // ángulo acumulado interpolado (para dibujo)
const interpolAngleDisplay = ref(0) // ángulo real interpolado (para indicador numérico)
const interpolPos = ref(45.0)
let animFrameId = null

// Ángulo acumulado (unwrap) para evitar que el dibujo "se devuelva" en 360→0
const anguloAcumulado = ref(0)
let prevRawAngle = null

function lerp(a, b, t) {
  return a + (b - a) * t
}

function animLoop() {
  // Actualizar ángulo acumulado (unwrap: detecta saltos >180° y acumula)
  const rawAngle = store.telemetria.angle
  if (prevRawAngle === null) {
    anguloAcumulado.value = rawAngle
  } else {
    let diff = rawAngle - prevRawAngle
    if (diff > 180) diff -= 360
    if (diff < -180) diff += 360
    anguloAcumulado.value += diff
  }
  prevRawAngle = rawAngle

  // Interpolar hacia el valor real (~suavizado exponencial)
  const t = 0.25  // factor de suavizado (más alto = más rápido)
  interpolAngle.value = lerp(interpolAngle.value, anguloAcumulado.value, t)
  interpolAngleDisplay.value = lerp(interpolAngleDisplay.value, rawAngle, t)
  interpolPos.value = lerp(interpolPos.value, store.telemetria.pos_ir_cm, t)
  animFrameId = requestAnimationFrame(animLoop)
}

onMounted(() => {
  animFrameId = requestAnimationFrame(animLoop)
})

onUnmounted(() => {
  if (animFrameId) cancelAnimationFrame(animFrameId)
})

// ── Posición del carro (px) ──────────────────
const carroX = computed(() => {
  const pos = Math.max(0, Math.min(PISTA_CM, interpolPos.value))
  return pistaX + (pos / PISTA_CM) * pistaW
})

const pivoteY = computed(() => pistaY - carroH)

// ── Punta del péndulo ────────────────────────
// Ángulo: 0° = arriba, 180° = abajo
// En SVG: arriba es Y negativo
const penduloPuntaX = computed(() => {
  const rad = (interpolAngle.value * Math.PI) / 180
  return carroX.value - barraLen * Math.sin(rad)
})

const penduloPuntaY = computed(() => {
  const rad = (interpolAngle.value * Math.PI) / 180
  return pivoteY.value - barraLen * Math.cos(rad)
  // Cuando angle=0 (arriba): cos(0)=1, pivoteY - 100 (arriba ✓)
  // Cuando angle=180 (abajo): cos(π)=-1, pivoteY + 100 (abajo ✓)
})

// ── Display formateado ───────────────────────
const anguloDisplay = computed(() => interpolAngleDisplay.value.toFixed(1))
const posicionDisplay = computed(() => interpolPos.value.toFixed(1))
</script>
