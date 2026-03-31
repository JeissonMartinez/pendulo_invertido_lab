<template>
  <div class="tab-rango-pwm">

    <!-- ═══════════ FILA SUPERIOR: CONTROLES + INDICADORES ═══════════ -->
    <div class="sweep-top-row">

      <!-- Controles del barrido -->
      <div class="card sweep-card-controles">
        <div class="card-header">Barrido PWM</div>
        <div class="card-body sweep-controles">

          <!-- Estado y Progreso en fila -->
          <div class="controles-fila">
            <div class="sweep-estado">
              <span class="control-label">Estado</span>
              <span class="badge" :class="claseBadgeSweep">{{ estadoSweepTexto }}</span>
            </div>

            <div class="sweep-progreso">
              <div class="progreso-info">
                <span class="control-label">Progreso</span>
                <span class="progreso-texto">
                  PWM {{ store.sweepState.currentPwm }} / {{ store.sweepState.totalSteps }}
                </span>
              </div>
              <div class="barra-progreso">
                <div class="barra-progreso-fill" :style="{ width: progresoPercent + '%' }"></div>
              </div>
              <span class="progreso-pct">{{ progresoPercent.toFixed(0) }}%</span>
            </div>
          </div>

          <!-- Rango detectado -->
          <div v-if="store.sweepState.pwmMin > 0" class="sweep-rango">
            <div class="rango-item">
              <span class="control-label">PWM Mínimo</span>
              <span class="rango-valor">{{ store.sweepState.pwmMin }}</span>
            </div>
            <div v-if="store.sweepState.pwmMax > 0" class="rango-item">
              <span class="control-label">PWM Máximo</span>
              <span class="rango-valor">{{ store.sweepState.pwmMax }}</span>
            </div>
          </div>

          <!-- Botones -->
          <div class="sweep-acciones">
            <button
              class="btn btn-iniciar"
              @click="store.iniciarBarrido()"
              :disabled="!puedeIniciar"
            >
              ▶ INICIAR
            </button>

            <button
              class="btn btn-detener"
              @click="store.detenerBarrido()"
              :disabled="!puedeDetener"
            >
              ⬛ DETENER
            </button>

            <button
              v-if="store.sweepResults"
              class="btn btn-reiniciar"
              @click="store.iniciarBarrido()"
            >
              ⟳ REINICIAR
            </button>

            <div class="sweep-info">
              <span class="control-label">Ciclos</span>
              <span class="info-valor">{{ store.sweepState.recordsCount }}</span>
            </div>
          </div>
        </div>
      </div>

      <!-- Indicadores -->
      <div class="card sweep-card-indicadores">
        <div class="card-header">Indicadores</div>
        <div class="card-body indicadores-sweep">

          <div class="indicadores-grid">
            <!-- Dirección -->
            <div class="indicador">
              <span class="indicador-label">Dirección</span>
              <span class="badge" :class="claseBadgeDireccion">
                {{ store.telemetria.direction }}
              </span>
            </div>

            <!-- PWM actual -->
            <div class="indicador">
              <span class="indicador-label">PWM</span>
              <span class="indicador-valor">{{ store.telemetria.pwm }}</span>
              <div class="barra-pwm">
                <div
                  class="barra-pwm-fill"
                  :style="{ width: (Math.abs(store.telemetria.pwm) / 1023 * 100) + '%' }"
                ></div>
              </div>
            </div>

            <!-- RPM -->
            <div class="indicador">
              <span class="indicador-label">RPM Motor</span>
              <span class="indicador-valor">{{ (store.telemetria.rpm ?? 0).toFixed(0) }}</span>
            </div>

            <!-- Posición estimada -->
            <div class="indicador">
              <span class="indicador-label">Posición</span>
              <span class="indicador-valor">
                {{ (store.telemetria.position ?? 0).toFixed(1) }} cm
              </span>
            </div>

            <!-- Limit Switches -->
            <div class="indicador">
              <span class="indicador-label">Limit Switches</span>
              <div class="ls-row">
                <span class="ls-led" :class="store.estado.limits.start_switch ? 'ls-activo' : 'ls-ok'"></span>
                <span class="ls-texto">Inicio</span>
                <span class="ls-led" :class="store.estado.limits.end_switch ? 'ls-activo' : 'ls-ok'" style="margin-left: 12px;"></span>
                <span class="ls-texto">Fin</span>
              </div>
            </div>

            <!-- WiFi -->
            <div class="indicador">
              <span class="indicador-label">WiFi RSSI</span>
              <span class="indicador-valor-sm">{{ store.estado.wifi_rssi }} dBm</span>
            </div>
          </div>

        </div>
      </div>

    </div>

    <!-- ═══════════ FILA CENTRAL: ANIMACIÓN + GRÁFICOS ═══════════ -->
    <div class="sweep-middle-row">
      <div class="animacion-wrapper">
        <AnimacionPendulo />
      </div>
      <div class="graficos-wrapper">
        <GraficosTelemetria :graficos-default="['angulo', 'rpm', 'posicion']" />
      </div>
    </div>

    <!-- ═══════════ FILA INFERIOR: RESULTADOS ═══════════ -->
    <div v-if="store.sweepResults" class="sweep-results-row">
      <ResultadosBarrido />
    </div>

  </div>
</template>

<script setup>
import { computed } from 'vue'
import { usePendulumStore } from '../../stores/pendulumStore'
import AnimacionPendulo from '../AnimacionPendulo.vue'
import GraficosTelemetria from '../GraficosTelemetria.vue'
import ResultadosBarrido from '../ResultadosBarrido.vue'

const store = usePendulumStore()

// Estados activos del barrido
const ESTADOS_ACTIVOS = ['HOMING', 'READY', 'FWD_RUNNING', 'SETTLE_END', 'PAUSE_AT_END', 'BACK_RUNNING', 'SETTLE_START', 'PAUSE_AT_START']

const esBarridoActivo = computed(() =>
  ESTADOS_ACTIVOS.includes(store.estadoTexto)
)

const puedeIniciar = computed(() =>
  store.wsConectado && !esBarridoActivo.value
)

const puedeDetener = computed(() =>
  store.wsConectado && esBarridoActivo.value
)

const progresoPercent = computed(() => {
  const { currentPwm, totalSteps } = store.sweepState
  if (totalSteps <= 0) return 0
  return Math.min((currentPwm / totalSteps) * 100, 100)
})

const estadoSweepTexto = computed(() => {
  const s = store.estadoTexto
  const mapa = {
    IDLE: 'Inactivo',
    HOMING: 'Buscando home...',
    READY: 'Listo',
    FWD_RUNNING: 'Adelante',
    SETTLE_END: 'Asentando en fin',
    PAUSE_AT_END: 'Pausa en fin',
    BACK_RUNNING: 'Regresando',
    SETTLE_START: 'Asentando en inicio',
    PAUSE_AT_START: 'Pausa en inicio',
    SHOW_RESULTS: 'Resultados',
    WAIT_REINICIAR: 'Esperando',
  }
  return mapa[s] || s
})

const claseBadgeSweep = computed(() => {
  const s = store.estadoTexto
  if (s === 'IDLE' || s === 'WAIT_REINICIAR') return 'badge-idle'
  if (s === 'READY' || s === 'SHOW_RESULTS') return 'badge-listo'
  if (s === 'HOMING') return 'badge-calibrando'
  if (ESTADOS_ACTIVOS.includes(s)) return 'badge-controlando'
  if (s === 'EMERGENCY') return 'badge-emergencia'
  return 'badge-idle'
})

const claseBadgeDireccion = computed(() => {
  const d = store.telemetria.direction
  if (d === 'ADELANTE') return 'badge-adelante'
  if (d === 'ATRAS') return 'badge-atras'
  return 'badge-idle'
})
</script>

<style scoped>
.tab-rango-pwm {
  display: flex;
  flex-direction: column;
  gap: 16px;
  padding: 16px;
  height: 100%;
  overflow-y: auto;
}

/* ── Fila superior: controles + indicadores ── */
.sweep-top-row {
  display: flex;
  gap: 16px;
  flex-shrink: 0;
}

.sweep-card-controles {
  flex: 1 1 55%;
  min-width: 0;
}

.sweep-card-indicadores {
  flex: 1 1 45%;
  min-width: 0;
}

.sweep-controles {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.controles-fila {
  display: flex;
  gap: 16px;
  align-items: flex-start;
}

.sweep-estado {
  display: flex;
  flex-direction: column;
  gap: 4px;
  flex-shrink: 0;
}

.sweep-progreso {
  display: flex;
  flex-direction: column;
  gap: 4px;
  flex: 1;
}

.progreso-info {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.progreso-texto {
  font-size: 0.78rem;
  color: var(--md-on-surface);
  font-weight: 500;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.progreso-pct {
  font-size: 0.72rem;
  color: var(--md-on-surface-secondary);
  text-align: right;
}

.barra-progreso {
  height: 8px;
  background: var(--md-outline);
  border-radius: 4px;
  overflow: hidden;
}

.barra-progreso-fill {
  height: 100%;
  background: linear-gradient(90deg, #51127C, #B5367A, #FC8961);
  border-radius: 4px;
  transition: width 0.3s ease;
}

.sweep-rango {
  display: flex;
  gap: 12px;
}

.rango-item {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 8px;
  background: #F3E8F9;
  border-radius: 6px;
}

.rango-valor {
  font-size: 1.2rem;
  font-weight: 700;
  color: #8C2981;
}

.sweep-acciones {
  display: flex;
  gap: 8px;
  align-items: center;
  flex-wrap: wrap;
}

.sweep-info {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 14px;
  background: var(--md-surface-dim);
  border-radius: 6px;
  flex-shrink: 0;
}

.info-valor {
  font-size: 1rem;
  font-weight: 600;
  color: var(--md-on-surface);
}

/* ── Botones ────────────────────────────────── */
.btn {
  border: none;
  border-radius: 8px;
  padding: 10px 16px;
  font-size: 0.85rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
  text-align: center;
  white-space: nowrap;
}

.btn:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.btn-iniciar {
  background: #43A047;
  color: white;
}

.btn-iniciar:hover:not(:disabled) {
  background: #388E3C;
}

.btn-detener {
  background: #E53935;
  color: white;
  letter-spacing: 0.03em;
}

.btn-detener:hover:not(:disabled) {
  background: #C62828;
}

.btn-reiniciar {
  background: #F3E8F9;
  color: #8C2981;
}

.btn-reiniciar:hover {
  background: #E8D0F4;
}

/* ── Indicadores ────────────────────────────── */
.indicadores-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
  gap: 10px;
}

.indicador {
  padding: 10px 12px;
  background: var(--md-surface-dim);
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.indicador-label {
  font-size: 0.72rem;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  font-weight: 600;
}

.indicador-valor {
  font-size: 1.2rem;
  font-weight: 700;
  color: #8C2981;
}

.indicador-valor-sm {
  font-size: 0.9rem;
  font-weight: 600;
  color: var(--md-on-surface);
}

.barra-pwm {
  height: 6px;
  background: var(--md-outline);
  border-radius: 3px;
  overflow: hidden;
  margin-top: 4px;
}

.barra-pwm-fill {
  height: 100%;
  background: linear-gradient(90deg, #51127C, #B5367A);
  border-radius: 3px;
  transition: width 0.1s ease;
}

/* ── Fila central: animación + gráficos ────── */
.sweep-middle-row {
  display: flex;
  gap: 16px;
  min-height: 340px;
  flex-shrink: 0;
}

.animacion-wrapper {
  flex: 1 1 50%;
  min-width: 0;
  min-height: 340px;
}

.graficos-wrapper {
  flex: 1 1 50%;
  min-width: 0;
  min-height: 0;
  overflow-y: auto;
}

/* ── Fila inferior: resultados ──────────────── */
.sweep-results-row {
  flex-shrink: 0;
}

/* ── Badges ─────────────────────────────────── */
.badge {
  display: inline-block;
  padding: 3px 10px;
  border-radius: 12px;
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  width: fit-content;
}

.badge-idle { background: #E8E0EC; color: #6E5F72; }
.badge-calibrando { background: #FDECDA; color: #C4501A; }
.badge-listo { background: #E8F5E9; color: #2E7D32; }
.badge-controlando { background: #F3E8F9; color: #8C2981; }
.badge-emergencia { background: #FFEBEE; color: #C62828; animation: pulsar 1s ease-in-out infinite; }
.badge-adelante { background: #E8F5E9; color: #2E7D32; }
.badge-atras { background: #FDECDA; color: #C4501A; }

@keyframes pulsar {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

/* ── Limit switches ─────────────────────────── */
.ls-row {
  display: flex;
  align-items: center;
  gap: 4px;
}

.ls-led {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  display: inline-block;
}

.ls-ok { background: #43A047; }
.ls-activo { background: #E53935; animation: pulsar 0.8s infinite; }
.ls-texto { font-size: 0.72rem; color: var(--md-on-surface-secondary); }

/* ── Control label ──────────────────────────── */
.control-label {
  font-size: 0.72rem;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

/* ── Responsive ─────────────────────────────── */
@media (max-width: 900px) {
  .sweep-top-row {
    flex-direction: column;
  }
  .sweep-middle-row {
    flex-direction: column;
  }
  .controles-fila {
    flex-direction: column;
  }
}
</style>
