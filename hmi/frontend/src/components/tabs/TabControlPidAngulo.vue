<template>
  <div class="tab-pid-angulo">

    <!-- ═══════════ FILA SUPERIOR: CONTROLES + ANIMACIÓN ═══════════ -->
    <div class="pid-top-row">

      <!-- Controles PID Ángulo -->
      <div class="card pid-card-controles">
        <div class="card-header">Control PID Ángulo</div>
        <div class="card-body pid-controles">

          <!-- Grid de controles superiores: Calibración + Posicionamiento -->
          <div class="pid-grid-superior">

            <!-- Calibración -->
            <div class="pid-seccion">
              <span class="control-label">Calibración</span>
              <div class="seccion-botones">
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('angle')"
                  :disabled="!store.wsConectado || esPIDAActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_angle }"
                >
                  🎯 Ángulo
                </button>
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('position')"
                  :disabled="!store.wsConectado || esPIDAActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_position }"
                >
                  📏 Pos. Mag.
                </button>
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('mpu')"
                  :disabled="!store.wsConectado || esPIDAActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_mpu }"
                >
                  🔄 MPU
                </button>
              </div>
            </div>

            <!-- Posicionamiento -->
            <div class="pid-seccion">
              <span class="control-label">Posicionamiento</span>
              <div class="seccion-botones">
                <button
                  class="btn btn-home"
                  @click="store.homePidAng()"
                  :disabled="!puedeComando"
                >
                  ⬅ Home
                </button>
                <button
                  class="btn btn-end"
                  @click="store.endPidAng()"
                  :disabled="!puedeComando"
                >
                  End ➡
                </button>
              </div>
            </div>

          </div>

          <!-- Sección Parámetros PID -->
          <div class="pid-seccion-params">
            <span class="control-label">Parámetros PID Ángulo</span>

            <div class="params-grid">
              <!-- Kp -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Kp</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kpVal"
                    min="0.1"
                    max="50"
                    step="0.1"
                    :disabled="esPIDAActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kpVal"
                  min="0.1"
                  max="50"
                  step="0.1"
                  :disabled="esPIDAActivo"
                />
                <div class="param-range-labels"><span>0.1</span><span>50</span></div>
              </div>

              <!-- Ki -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Ki</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kiVal"
                    min="0"
                    max="10"
                    step="0.01"
                    :disabled="esPIDAActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kiVal"
                  min="0"
                  max="10"
                  step="0.01"
                  :disabled="esPIDAActivo"
                />
                <div class="param-range-labels"><span>0</span><span>10</span></div>
              </div>

              <!-- Kd -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Kd</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kdVal"
                    min="0"
                    max="5"
                    step="0.01"
                    :disabled="esPIDAActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kdVal"
                  min="0"
                  max="5"
                  step="0.01"
                  :disabled="esPIDAActivo"
                />
                <div class="param-range-labels"><span>0</span><span>5</span></div>
              </div>
            </div>
          </div>

          <!-- Sección Control (Rango Trabajo, Tolerancia) -->
          <div class="pid-seccion-control">
            <span class="control-label">Control Angular</span>

            <div class="control-grid">
              <!-- Rango de Trabajo -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Rango Trabajo (°)</span>
                  <input
                    type="number"
                    class="param-input param-input-lg"
                    v-model.number="rangoTrabajoVal"
                    min="5"
                    max="30"
                    step="1"
                    :disabled="esPIDAActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="rangoTrabajoVal"
                  min="5"
                  max="30"
                  step="1"
                  :disabled="esPIDAActivo"
                />
                <div class="param-range-labels"><span>5°</span><span>30°</span></div>
              </div>

              <!-- Tolerancia -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Tolerancia (°)</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="toleranciaVal"
                    min="0"
                    max="5"
                    step="0.1"
                    :disabled="esPIDAActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="toleranciaVal"
                  min="0"
                  max="5"
                  step="0.1"
                  :disabled="esPIDAActivo"
                />
                <div class="param-range-labels"><span>1°</span><span>10°</span></div>
              </div>
            </div>
          </div>

          <!-- Sección Motor (PWM Min/Max) -->
          <div class="pid-seccion-motor">
            <span class="control-label">Motor PWM</span>

            <div class="motor-grid">
              <!-- PWM Min -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">PWM Mín</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="pwmMinVal"
                    min="100"
                    max="500"
                    step="10"
                    :disabled="esPIDAActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="pwmMinVal"
                  min="100"
                  max="500"
                  step="10"
                  :disabled="esPIDAActivo"
                />
                <div class="param-range-labels"><span>100</span><span>500</span></div>
              </div>

              <!-- PWM Max -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">PWM Máx</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="pwmMaxVal"
                    min="300"
                    max="1023"
                    step="10"
                    :disabled="esPIDAActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="pwmMaxVal"
                  min="300"
                  max="1023"
                  step="10"
                  :disabled="esPIDAActivo"
                />
                <div class="param-range-labels"><span>300</span><span>1023</span></div>
              </div>
            </div>
          </div>

          <!-- Botones de acción -->
          <div class="pid-acciones">
            <button
              class="btn btn-config"
              @click="enviarConfiguracion"
              :disabled="!puedeConfigurar"
            >
              ⚙ ENVIAR CONFIG
            </button>
            <button
              class="btn btn-ejecutar"
              @click="store.ejecutarPidAng()"
              :disabled="!puedeEjecutar"
            >
              ▶ EJECUTAR
            </button>
            <button
              class="btn btn-detener"
              @click="store.detenerPidAng()"
              :disabled="!store.wsConectado"
            >
              ⬛ DETENER
            </button>
          </div>

        </div>
      </div>

      <!-- Animación del Péndulo -->
      <div class="animacion-wrapper">
        <AnimacionPendulo />
      </div>

    </div>

    <!-- ═══════════ FILA INDICADORES (ancho completo) ═══════════ -->
    <div class="pid-indicators-row">
      <div class="card pid-card-indicadores">
        <div class="card-header">Indicadores</div>
        <div class="card-body">
          <IndicadoresTelemetria />
          
          <!-- Sección adicional: Indicadores PID Ángulo -->
          <section class="seccion seccion-pid">
            <h4 class="seccion-titulo">Control PID Ángulo</h4>
            <div class="pid-indicadores-grid">
              <div class="pid-indicadores-col">
                <div class="indicadores-row">
                  <div class="indicador indicador-destacado">
                    <span class="indicador-label">Error</span>
                    <span class="indicador-valor-lg" :class="errorClass">{{ fmt(t.pida_error, 2) }}</span>
                    <span class="indicador-unidad">°</span>
                  </div>
                  <div class="indicador indicador-destacado">
                    <span class="indicador-label">Output</span>
                    <span class="indicador-valor-lg">{{ fmt(t.pida_output, 0) }}</span>
                  </div>
                  <div class="indicador indicador-destacado">
                    <span class="indicador-label">Output Sat</span>
                    <span class="indicador-valor-lg">{{ fmt(t.pida_output_sat, 0) }}</span>
                  </div>
                </div>
              </div>
              <div class="pid-indicadores-col">
                <div class="indicadores-row indicadores-2col">
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Integral</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pida_integral, 1) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Derivada</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pida_derivada, 2) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Setpoint</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pida_setpoint, 1) }}°</span>
                  </div>
                  <div class="indicador">
                    <span class="indicador-label">Estado</span>
                    <span class="badge" :class="claseBadgePidaState">{{ pidaStateText }}</span>
                  </div>
                </div>
                <div class="indicadores-row indicadores-3col">
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Kp</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pida_kp, 2) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Ki</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pida_ki, 3) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Kd</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pida_kd, 3) }}</span>
                  </div>
                </div>
              </div>
            </div>
          </section>

        </div>
      </div>
    </div>

    <!-- ═══════════ FILA CENTRAL: GRÁFICOS ═══════════ -->
    <div class="pid-middle-row">
      <GraficosTelemetria 
        :graficos-default="['angulo', 'posicion', 'rpm', 'pwm', 'pida_output', 'pida_error']" 
        layout="grid"
      />
    </div>

    <!-- ═══════════ FILA INFERIOR: RESULTADOS ═══════════ -->
    <div class="pid-results-row">
      <ResultadosPidAngulo />
    </div>

  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { usePendulumStore } from '../../stores/pendulumStore'
import AnimacionPendulo from '../AnimacionPendulo.vue'
import GraficosTelemetria from '../GraficosTelemetria.vue'
import IndicadoresTelemetria from '../IndicadoresTelemetria.vue'
import ResultadosPidAngulo from '../ResultadosPidAngulo.vue'

const store = usePendulumStore()

// Valores locales de configuración
const kpVal = ref(store.pidAngState.kp)
const kiVal = ref(store.pidAngState.ki)
const kdVal = ref(store.pidAngState.kd)
const rangoTrabajoVal = ref(store.pidAngState.rango_trabajo)
const toleranciaVal = ref(store.pidAngState.tolerancia)
const pwmMinVal = ref(store.pidAngState.pwm_min)
const pwmMaxVal = ref(store.pidAngState.pwm_max)

// Telemetría reactiva
const t = computed(() => store.telemetria)

// Estados activos del PID Ángulo (motor en movimiento)
const ESTADOS_ACTIVOS_PIDA = [
  'PIDA_HOMING_START', 'PIDA_HOMING_END',
  'PIDA_PREPOSITIONING', 'PIDA_RUNNING', 'PIDA_REPOSITIONING'
]
// Estados donde se puede ejecutar el control PID
const ESTADOS_READY_PIDA = ['PIDA_IDLE', 'PIDA_READY', 'PIDA_STOPPED']

const esPIDAActivo = computed(() =>
  ESTADOS_ACTIVOS_PIDA.includes(store.pidAngState.status)
)

const puedeComando = computed(() =>
  store.wsConectado && !esPIDAActivo.value
)

const puedeConfigurar = computed(() =>
  store.wsConectado && 
  !esPIDAActivo.value &&
  pwmMinVal.value < pwmMaxVal.value
)

const puedeEjecutar = computed(() =>
  store.wsConectado &&
  ESTADOS_READY_PIDA.includes(store.pidAngState.status)
)

// Texto del estado PID Ángulo
const PIDA_STATE_TEXT = {
  PIDA_IDLE: 'INACTIVO',
  PIDA_HOMING_START: 'HOMING →',
  PIDA_HOMING_END: '← HOMING',
  PIDA_CALIBRATING: 'CALIBRANDO',
  PIDA_PREPOSITIONING: 'PRE-POS',
  PIDA_READY: 'ESPERANDO',
  PIDA_RUNNING: 'CONTROLANDO',
  PIDA_REPOSITIONING: 'REPOSICIONANDO',
  PIDA_STOPPED: 'DETENIDO',
}

const pidaStateText = computed(() => {
  const st = store.pidAngState.status
  return PIDA_STATE_TEXT[st] ?? st ?? 'INACTIVO'
})

const claseBadgePidaState = computed(() => {
  const mapa = {
    PIDA_IDLE: 'badge-idle',
    PIDA_HOMING_START: 'badge-calibrando',
    PIDA_HOMING_END: 'badge-calibrando',
    PIDA_CALIBRATING: 'badge-calibrando',
    PIDA_PREPOSITIONING: 'badge-calibrando',
    PIDA_READY: 'badge-listo',
    PIDA_RUNNING: 'badge-controlando',
    PIDA_REPOSITIONING: 'badge-calibrando',
    PIDA_STOPPED: 'badge-detenido',
  }
  return mapa[store.pidAngState.status] || 'badge-idle'
})

// Clase para colorear el error según magnitud
const errorClass = computed(() => {
  const absErr = Math.abs(t.value.pida_error)
  if (absErr < toleranciaVal.value) return 'error-ok'
  if (absErr < toleranciaVal.value * 3) return 'error-near'
  return 'error-far'
})

// Función de formateo
function fmt(val, decimals) {
  return (val ?? 0).toFixed(decimals)
}

// Enviar configuración al firmware
function enviarConfiguracion() {
  store.configurarPidAng({
    kp: kpVal.value,
    ki: kiVal.value,
    kd: kdVal.value,
    rango_trabajo: rangoTrabajoVal.value,
    tolerancia: toleranciaVal.value,
    pwm_min: pwmMinVal.value,
    pwm_max: pwmMaxVal.value,
  })
}
</script>

<style scoped>
.tab-pid-angulo {
  display: flex;
  flex-direction: column;
  gap: 16px;
  padding: 16px;
  height: 100%;
  overflow-y: auto;
}

/* ── Fila superior ── */
.pid-top-row {
  display: flex;
  gap: 16px;
  flex-shrink: 0;
  align-items: stretch;
}

.pid-card-controles {
  flex: 1 1 45%;
  min-width: 0;
  overflow: visible;
}

.animacion-wrapper {
  flex: 1 1 55%;
  min-width: 0;
  display: flex;
  align-items: stretch;
}

.animacion-wrapper :deep(.card) {
  flex: 1;
  display: flex;
  flex-direction: column;
}

.animacion-wrapper :deep(.card-body) {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
}

/* ── Fila de indicadores (ancho completo) ── */
.pid-indicators-row {
  flex-shrink: 0;
  min-width: 0;
}

.pid-card-indicadores {
  min-width: 0;
  overflow: visible;
}

/* ── PID Indicadores grid 2 columnas ── */
.pid-indicadores-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
}

.pid-indicadores-col {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.indicadores-2col {
  grid-template-columns: repeat(2, 1fr) !important;
}

.pid-controles {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

/* ── Grid superior ── */
.pid-grid-superior {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  align-items: flex-end;
}

.pid-seccion {
  flex: 1 1 auto;
  min-width: 100px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.seccion-botones {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}

/* ── Secciones de parámetros ── */
.pid-seccion-params,
.pid-seccion-control,
.pid-seccion-motor {
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding-top: 12px;
  border-top: 1px solid var(--md-outline-variant, #E0E0E0);
}

.params-grid,
.control-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 12px;
}

.motor-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
}

/* ── Parámetros (sliders) ── */
.pid-param {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.param-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.param-label {
  font-size: 0.72rem;
  font-weight: 600;
  color: var(--md-on-surface-secondary, #757575);
}

.param-input {
  width: 60px;
  padding: 4px 6px;
  border: 1px solid var(--md-outline);
  border-radius: 6px;
  font-size: 0.8rem;
  font-weight: 600;
  text-align: center;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
  color: #8C2981;
  background: var(--md-surface-dim);
}

.param-input-lg {
  width: 72px;
}

.param-input:focus {
  outline: none;
  border-color: #8C2981;
}

.param-input:disabled {
  opacity: 0.5;
}

.param-input.param-invalid {
  border-color: #E53935;
  background: #FFEBEE;
}

.param-slider {
  width: 100%;
  accent-color: #8C2981;
  height: 6px;
}

.param-slider:disabled {
  opacity: 0.5;
}

.param-range-labels {
  display: flex;
  justify-content: space-between;
  font-size: 0.65rem;
  color: var(--md-on-surface-secondary);
}

/* ── Botones de acción ── */
.pid-acciones {
  display: flex;
  gap: 10px;
  padding-top: 12px;
  border-top: 1px solid var(--md-outline-variant, #E0E0E0);
  flex-wrap: wrap;
}

.btn {
  border: none;
  border-radius: 8px;
  padding: 10px 16px;
  font-size: 0.85rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
  white-space: nowrap;
}

.btn:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.btn-calibrar {
  background: #F3E8F9;
  color: #8C2981;
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 12px;
  font-size: 0.78rem;
}

.btn-calibrar:hover:not(:disabled) {
  background: #E8D5F5;
}

.btn-calibrar.cal-ok {
  background: #E8F5E9;
  color: #2E7D32;
  border: 1px solid #A5D6A7;
}

.btn-home {
  background: #E3F2FD;
  color: #1565C0;
}

.btn-home:hover:not(:disabled) {
  background: #BBDEFB;
}

.btn-end {
  background: #FFF3E0;
  color: #E65100;
}

.btn-end:hover:not(:disabled) {
  background: #FFE0B2;
}

.btn-config {
  background: #E8EAF6;
  color: #3949AB;
  flex: 1;
}

.btn-config:hover:not(:disabled) {
  background: #C5CAE9;
}

.btn-ejecutar {
  background: #43A047;
  color: white;
  flex: 1;
}

.btn-ejecutar:hover:not(:disabled) {
  background: #388E3C;
}

.btn-detener {
  background: #E53935;
  color: white;
}

.btn-detener:hover:not(:disabled) {
  background: #C62828;
}

/* ── Control label ── */
.control-label {
  font-size: 0.72rem;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

/* ── Sección indicadores PID ── */
.seccion-pid {
  margin-top: 12px;
  padding-top: 12px;
  border-top: 1px solid var(--md-outline-variant);
}

.seccion-titulo {
  margin: 0 0 8px 0;
  font-size: 0.7rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #7B1FA2;
  padding-bottom: 4px;
  border-bottom: 1px solid #E1BEE7;
}

.indicadores-row {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin-bottom: 8px;
}

.indicador {
  flex: 1 1 auto;
  min-width: 60px;
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 6px 8px;
  background: var(--md-surface-dim, #F5F5F5);
  border-radius: 6px;
}

.indicador-destacado {
  background: #F3E5F5;
}

.indicador-sm {
  min-width: 50px;
}

.indicador-label {
  font-size: 0.65rem;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.indicador-valor-lg {
  font-size: 1.3rem;
  font-weight: 700;
  font-family: 'JetBrains Mono', monospace;
  color: #7B1FA2;
}

.indicador-valor-sm {
  font-size: 0.9rem;
  font-weight: 600;
  font-family: 'JetBrains Mono', monospace;
  color: var(--md-on-surface);
}

.indicador-unidad {
  font-size: 0.65rem;
  color: var(--md-on-surface-secondary);
}

/* Colores del error */
.error-ok { color: #2E7D32 !important; }
.error-near { color: #F57C00 !important; }
.error-far { color: #C62828 !important; }

/* Badges */
.badge {
  font-size: 0.7rem;
  font-weight: 600;
  padding: 3px 8px;
  border-radius: 4px;
  text-transform: uppercase;
  letter-spacing: 0.03em;
}

.badge-idle { background: #ECEFF1; color: #607D8B; }
.badge-calibrando { background: #FFF3E0; color: #E65100; }
.badge-listo { background: #E8F5E9; color: #2E7D32; }
.badge-controlando { background: #E3F2FD; color: #1565C0; }
.badge-detenido { background: #FFEBEE; color: #C62828; }

/* ── Fila central: gráficos ── */
.pid-middle-row {
  flex: 0 0 auto;
  min-height: 380px;
  min-width: 0;
}

/* ── Fila inferior: resultados ── */
.pid-results-row {
  flex-shrink: 0;
  margin-top: auto;
  min-width: 0;
}

/* ── Responsive ── */
@media (max-width: 1200px) {
  .pid-top-row {
    flex-wrap: wrap;
  }
  .pid-card-controles {
    flex: 1 1 100%;
  }
  .animacion-wrapper {
    flex: 1 1 100%;
  }
  .params-grid,
  .control-grid {
    grid-template-columns: repeat(3, 1fr);
  }
}

@media (max-width: 800px) {
  .pid-top-row {
    flex-direction: column;
  }
  .pid-indicadores-grid {
    grid-template-columns: 1fr;
  }
  .params-grid,
  .control-grid {
    grid-template-columns: repeat(2, 1fr);
  }
  .motor-grid {
    grid-template-columns: 1fr 1fr;
  }
}

@media (max-width: 500px) {
  .params-grid,
  .control-grid,
  .motor-grid {
    grid-template-columns: 1fr;
  }
}
</style>
