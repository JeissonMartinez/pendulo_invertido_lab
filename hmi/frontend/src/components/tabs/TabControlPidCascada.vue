<template>
  <div class="tab-pid-cascada">

    <!-- ═══════════ FILA SUPERIOR: CONTROLES + ANIMACIÓN ═══════════ -->
    <div class="pid-top-row">

      <!-- Controles PID Cascada -->
      <div class="card pid-card-controles">
        <div class="card-header">Control PID Cascada</div>
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
                  :disabled="!store.wsConectado || esPIDCActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_angle }"
                >
                  🎯 Ángulo
                </button>
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('position')"
                  :disabled="!store.wsConectado || esPIDCActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_position }"
                >
                  📏 Pos. Mag.
                </button>
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('mpu')"
                  :disabled="!store.wsConectado || esPIDCActivo"
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
                  @click="store.homePidCasc()"
                  :disabled="!puedeComando"
                >
                  ⬅ Home
                </button>
                <button
                  class="btn btn-end"
                  @click="store.endPidCasc()"
                  :disabled="!puedeComando"
                >
                  End ➡
                </button>
              </div>
            </div>

          </div>

          <!-- ═══ Sección Parámetros PID Posición (lazo externo) ═══ -->
          <div class="pid-seccion-params">
            <span class="control-label">Lazo Externo — PID Posición → θ_ref</span>

            <div class="params-grid">
              <!-- Kp Pos -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Kp Pos</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kpPosVal"
                    min="0"
                    max="20"
                    step="0.01"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kpPosVal"
                  min="0"
                  max="20"
                  step="0.01"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0</span><span>20</span></div>
              </div>

              <!-- Ki Pos -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Ki Pos</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kiPosVal"
                    min="0"
                    max="5"
                    step="0.001"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kiPosVal"
                  min="0"
                  max="5"
                  step="0.001"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0</span><span>5</span></div>
              </div>

              <!-- Kd Pos -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Kd Pos</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kdPosVal"
                    min="0"
                    max="5"
                    step="0.01"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kdPosVal"
                  min="0"
                  max="5"
                  step="0.01"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0</span><span>5</span></div>
              </div>
            </div>

            <!-- Setpoint Posición + Zona Seguridad + θ_ref max -->
            <div class="control-grid">
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Setpoint Pos (cm)</span>
                  <input
                    type="number"
                    class="param-input param-input-lg"
                    v-model.number="posSetpointVal"
                    min="5"
                    max="85"
                    step="1"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="posSetpointVal"
                  min="5"
                  max="85"
                  step="1"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>5cm</span><span>85cm</span></div>
              </div>

              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Zona Seg (cm)</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="zonaSeguridadVal"
                    min="2"
                    max="20"
                    step="1"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="zonaSeguridadVal"
                  min="2"
                  max="20"
                  step="1"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>2cm</span><span>20cm</span></div>
              </div>

              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">θ_ref máx (°)</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="thetaRefMaxVal"
                    min="0.5"
                    max="10"
                    step="0.1"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="thetaRefMaxVal"
                  min="0.5"
                  max="10"
                  step="0.1"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0.5°</span><span>10°</span></div>
              </div>
            </div>
          </div>

          <!-- ═══ Sección Parámetros PID Ángulo (lazo interno) ═══ -->
          <div class="pid-seccion-params">
            <span class="control-label">Lazo Interno — PID Ángulo → PWM</span>

            <div class="params-grid">
              <!-- Kp Ang -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Kp Áng</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kpAngVal"
                    min="0.1"
                    max="100"
                    step="0.1"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kpAngVal"
                  min="0.1"
                  max="100"
                  step="0.1"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0.1</span><span>100</span></div>
              </div>

              <!-- Ki Ang -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Ki Áng</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kiAngVal"
                    min="0"
                    max="50"
                    step="0.01"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kiAngVal"
                  min="0"
                  max="50"
                  step="0.01"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0</span><span>50</span></div>
              </div>

              <!-- Kd Ang -->
              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Kd Áng</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="kdAngVal"
                    min="0"
                    max="10"
                    step="0.01"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="kdAngVal"
                  min="0"
                  max="10"
                  step="0.01"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0</span><span>10</span></div>
              </div>
            </div>
          </div>

          <!-- Sección Control Angular (Rango Trabajo, Tolerancia) -->
          <div class="pid-seccion-control">
            <span class="control-label">Control Angular</span>

            <div class="control-grid">
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
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="rangoTrabajoVal"
                  min="5"
                  max="30"
                  step="1"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>5°</span><span>30°</span></div>
              </div>

              <div class="pid-param">
                <div class="param-header">
                  <span class="param-label">Tolerancia (°)</span>
                  <input
                    type="number"
                    class="param-input"
                    v-model.number="toleranciaAngVal"
                    min="0"
                    max="5"
                    step="0.1"
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="toleranciaAngVal"
                  min="0"
                  max="5"
                  step="0.1"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>0°</span><span>5°</span></div>
              </div>
            </div>
          </div>

          <!-- Sección Motor PWM -->
          <div class="pid-seccion-motor">
            <span class="control-label">Motor PWM</span>

            <div class="motor-grid">
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
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="pwmMinVal"
                  min="100"
                  max="500"
                  step="10"
                  :disabled="esPIDCActivo"
                />
                <div class="param-range-labels"><span>100</span><span>500</span></div>
              </div>

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
                    :disabled="esPIDCActivo"
                  />
                </div>
                <input
                  type="range"
                  class="param-slider"
                  v-model.number="pwmMaxVal"
                  min="300"
                  max="1023"
                  step="10"
                  :disabled="esPIDCActivo"
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
              @click="store.ejecutarPidCasc()"
              :disabled="!puedeEjecutar"
            >
              ▶ EJECUTAR
            </button>
            <button
              class="btn btn-detener"
              @click="store.detenerPidCasc()"
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
        <div class="card-header">Indicadores Cascada</div>
        <div class="card-body">
          <IndicadoresTelemetria />
          
          <!-- Indicadores Lazo Externo (Posición) -->
          <section class="seccion seccion-pid">
            <h4 class="seccion-titulo">Lazo Externo — Posición</h4>
            <div class="pid-indicadores-grid">
              <div class="pid-indicadores-col">
                <div class="indicadores-row">
                  <div class="indicador indicador-destacado indicador-pos">
                    <span class="indicador-label">Error Pos</span>
                    <span class="indicador-valor-lg">{{ fmt(t.pidc_pos_error, 2) }}</span>
                    <span class="indicador-unidad">cm</span>
                  </div>
                  <div class="indicador indicador-destacado indicador-pos">
                    <span class="indicador-label">θ_ref</span>
                    <span class="indicador-valor-lg" :class="thetaRefClass">{{ fmt(t.pidc_pos_output_sat, 3) }}</span>
                    <span class="indicador-unidad">°</span>
                  </div>
                  <div class="indicador indicador-destacado indicador-pos">
                    <span class="indicador-label">Setpoint</span>
                    <span class="indicador-valor-lg">{{ fmt(t.pidc_pos_setpoint, 1) }}</span>
                    <span class="indicador-unidad">cm</span>
                  </div>
                </div>
              </div>
              <div class="pid-indicadores-col">
                <div class="indicadores-row indicadores-2col">
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Integral</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_pos_integral, 1) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Derivada</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_pos_derivada, 2) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Output crudo</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_pos_output, 2) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Zona Seg</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_pos_zona_seg, 1) }} cm</span>
                  </div>
                </div>
              </div>
            </div>
          </section>

          <!-- Indicadores Lazo Interno (Ángulo) -->
          <section class="seccion seccion-pid">
            <h4 class="seccion-titulo seccion-titulo-ang">Lazo Interno — Ángulo</h4>
            <div class="pid-indicadores-grid">
              <div class="pid-indicadores-col">
                <div class="indicadores-row">
                  <div class="indicador indicador-destacado">
                    <span class="indicador-label">Error Áng</span>
                    <span class="indicador-valor-lg" :class="errorAngClass">{{ fmt(t.pidc_ang_error, 2) }}</span>
                    <span class="indicador-unidad">°</span>
                  </div>
                  <div class="indicador indicador-destacado">
                    <span class="indicador-label">Output</span>
                    <span class="indicador-valor-lg">{{ fmt(t.pidc_ang_output, 0) }}</span>
                  </div>
                  <div class="indicador indicador-destacado">
                    <span class="indicador-label">Output Sat</span>
                    <span class="indicador-valor-lg">{{ fmt(t.pidc_ang_output_sat, 0) }}</span>
                  </div>
                </div>
              </div>
              <div class="pid-indicadores-col">
                <div class="indicadores-row indicadores-2col">
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Integral</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_ang_integral, 1) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Derivada</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_ang_derivada, 2) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Setpoint(θ_ref)</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_ang_setpoint, 3) }}°</span>
                  </div>
                  <div class="indicador">
                    <span class="indicador-label">Estado</span>
                    <span class="badge" :class="claseBadgePidcState">{{ pidcStateText }}</span>
                  </div>
                </div>
                <div class="indicadores-row indicadores-3col">
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Kp Áng</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_ang_kp, 2) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Ki Áng</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_ang_ki, 3) }}</span>
                  </div>
                  <div class="indicador indicador-sm">
                    <span class="indicador-label">Kd Áng</span>
                    <span class="indicador-valor-sm">{{ fmt(t.pidc_ang_kd, 3) }}</span>
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
        :graficos-default="['angulo', 'posicion', 'pwm', 'pidc_pos_output', 'pidc_pos_error', 'pidc_ang_output', 'pidc_ang_error']" 
        layout="grid"
      />
    </div>

    <!-- ═══════════ FILA INFERIOR: RESULTADOS ═══════════ -->
    <div class="pid-results-row">
      <ResultadosPidCascada />
    </div>

  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { usePendulumStore } from '../../stores/pendulumStore'
import AnimacionPendulo from '../AnimacionPendulo.vue'
import GraficosTelemetria from '../GraficosTelemetria.vue'
import IndicadoresTelemetria from '../IndicadoresTelemetria.vue'
import ResultadosPidCascada from '../ResultadosPidCascada.vue'

const store = usePendulumStore()

// Valores locales — Lazo externo (Posición)
const kpPosVal = ref(store.pidCascState.kp_pos)
const kiPosVal = ref(store.pidCascState.ki_pos)
const kdPosVal = ref(store.pidCascState.kd_pos)
const posSetpointVal = ref(store.pidCascState.pos_setpoint)
const zonaSeguridadVal = ref(store.pidCascState.zona_seguridad)
const thetaRefMaxVal = ref(store.pidCascState.theta_ref_max)

// Valores locales — Lazo interno (Ángulo)
const kpAngVal = ref(store.pidCascState.kp_ang)
const kiAngVal = ref(store.pidCascState.ki_ang)
const kdAngVal = ref(store.pidCascState.kd_ang)
const rangoTrabajoVal = ref(store.pidCascState.rango_trabajo)
const toleranciaAngVal = ref(store.pidCascState.tolerancia_ang)
const pwmMinVal = ref(store.pidCascState.pwm_min)
const pwmMaxVal = ref(store.pidCascState.pwm_max)

// Telemetría reactiva
const t = computed(() => store.telemetria)

// Estados activos del PID Cascada (motor en movimiento)
const ESTADOS_ACTIVOS_PIDC = [
  'PIDC_HOMING_START', 'PIDC_HOMING_END', 'PIDC_RUNNING'
]
// Estados donde se puede ejecutar
const ESTADOS_READY_PIDC = ['PIDC_IDLE', 'PIDC_READY', 'PIDC_STOPPED']

const esPIDCActivo = computed(() =>
  ESTADOS_ACTIVOS_PIDC.includes(store.pidCascState.status)
)

const puedeComando = computed(() =>
  store.wsConectado && !esPIDCActivo.value
)

const puedeConfigurar = computed(() =>
  store.wsConectado && 
  !esPIDCActivo.value &&
  pwmMinVal.value < pwmMaxVal.value
)

const puedeEjecutar = computed(() =>
  store.wsConectado &&
  ESTADOS_READY_PIDC.includes(store.pidCascState.status)
)

// Texto del estado PID Cascada
const PIDC_STATE_TEXT = {
  PIDC_IDLE: 'INACTIVO',
  PIDC_HOMING_START: 'HOMING →',
  PIDC_HOMING_END: '← HOMING',
  PIDC_CALIBRATING: 'CALIBRANDO',
  PIDC_WAIT_READY: 'ESPERANDO...',
  PIDC_READY: 'LISTO',
  PIDC_RUNNING: 'CONTROLANDO',
  PIDC_STOPPED: 'DETENIDO',
}

const pidcStateText = computed(() => {
  const st = store.pidCascState.status
  return PIDC_STATE_TEXT[st] ?? st ?? 'INACTIVO'
})

const claseBadgePidcState = computed(() => {
  const mapa = {
    PIDC_IDLE: 'badge-idle',
    PIDC_HOMING_START: 'badge-calibrando',
    PIDC_HOMING_END: 'badge-calibrando',
    PIDC_CALIBRATING: 'badge-calibrando',
    PIDC_WAIT_READY: 'badge-calibrando',
    PIDC_READY: 'badge-listo',
    PIDC_RUNNING: 'badge-controlando',
    PIDC_STOPPED: 'badge-detenido',
  }
  return mapa[store.pidCascState.status] || 'badge-idle'
})

// Clase para colorear θ_ref según cercanía a saturación
const thetaRefClass = computed(() => {
  const absRef = Math.abs(t.value.pidc_pos_output_sat)
  const max = thetaRefMaxVal.value || 2
  if (absRef < max * 0.5) return 'error-ok'
  if (absRef < max * 0.9) return 'error-near'
  return 'error-far'
})

// Clase para colorear el error angular
const errorAngClass = computed(() => {
  const absErr = Math.abs(t.value.pidc_ang_error)
  if (absErr < toleranciaAngVal.value) return 'error-ok'
  if (absErr < toleranciaAngVal.value * 3) return 'error-near'
  return 'error-far'
})

function fmt(val, decimals) {
  return (val ?? 0).toFixed(decimals)
}

function enviarConfiguracion() {
  store.configurarPidCasc({
    kp_pos: kpPosVal.value,
    ki_pos: kiPosVal.value,
    kd_pos: kdPosVal.value,
    pos_setpoint: posSetpointVal.value,
    zona_seguridad: zonaSeguridadVal.value,
    theta_ref_max: thetaRefMaxVal.value,
    kp_ang: kpAngVal.value,
    ki_ang: kiAngVal.value,
    kd_ang: kdAngVal.value,
    rango_trabajo: rangoTrabajoVal.value,
    tolerancia_ang: toleranciaAngVal.value,
    pwm_min: pwmMinVal.value,
    pwm_max: pwmMaxVal.value,
  })
}
</script>

<style scoped>
.tab-pid-cascada {
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

/* ── Fila de indicadores ── */
.pid-indicators-row {
  flex-shrink: 0;
  min-width: 0;
}

.pid-card-indicadores {
  min-width: 0;
  overflow: visible;
}

/* ── PID Indicadores grid ── */
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

.indicadores-3col {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 8px;
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
  color: #00695C;
  background: var(--md-surface-dim);
}

.param-input-lg {
  width: 72px;
}

.param-input:focus {
  outline: none;
  border-color: #00695C;
}

.param-input:disabled {
  opacity: 0.5;
}

.param-slider {
  width: 100%;
  accent-color: #00695C;
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
  background: #E0F2F1;
  color: #00695C;
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 12px;
  font-size: 0.78rem;
}

.btn-calibrar:hover:not(:disabled) {
  background: #B2DFDB;
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
  background: #E0F2F1;
  color: #00695C;
  flex: 1;
}

.btn-config:hover:not(:disabled) {
  background: #B2DFDB;
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
  color: #00695C;
  padding-bottom: 4px;
  border-bottom: 1px solid #80CBC4;
}

.seccion-titulo-ang {
  color: #7B1FA2;
  border-bottom-color: #E1BEE7;
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

.indicador-pos {
  background: #E0F2F1;
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
  color: #00695C;
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
