<template>
  <div class="tab-lazo-abierto">

    <!-- ═══════════ FILA SUPERIOR: CONTROLES + INDICADORES ═══════════ -->
    <div class="ol-top-row">

      <!-- Controles Lazo Abierto -->
      <div class="card ol-card-controles">
        <div class="card-header">Control Lazo Abierto</div>
        <div class="card-body ol-controles">

          <!-- Estado -->
          <div class="controles-fila">
            <div class="ol-estado">
              <span class="control-label">Estado</span>
              <span class="badge" :class="claseBadgeOL">{{ estadoOLTexto }}</span>
            </div>
            <div v-if="store.openLoopState.lastPosition" class="ol-posicion">
              <span class="control-label">Posición</span>
              <span class="badge badge-posicion">
                {{ store.openLoopState.lastPosition === 'start' ? '⬅ INICIO' : '➡ FIN' }}
              </span>
            </div>
          </div>

          <!-- Homing buttons -->
          <div class="ol-homing">
            <span class="control-label">Posicionamiento</span>
            <div class="homing-botones">
              <button
                class="btn btn-home"
                @click="store.homeCarro()"
                :disabled="!puedeComando"
              >
                ⬅ HOME (Inicio)
              </button>
              <button
                class="btn btn-end"
                @click="store.endCarro()"
                :disabled="!puedeComando"
              >
                END (Fin) ➡
              </button>
            </div>
          </div>

          <!-- Calibración de sensores -->
          <div class="ol-calibracion">
            <span class="control-label">Calibración</span>
            <div class="calibracion-botones">
              <button
                class="btn btn-calibrar"
                @click="store.calibrar('angle')"
                :disabled="!store.wsConectado || esOLActivo"
              >
                🎯 Ángulo
                <span class="cal-badge" :class="store.estado.calibrated_angle ? 'cal-ok' : 'cal-pending'">{{ store.estado.calibrated_angle ? '✔' : '⚠' }}</span>
              </button>
              <button
                class="btn btn-calibrar"
                @click="store.calibrar('position')"
                :disabled="!store.wsConectado || esOLActivo"
              >
                📏 Pos. Mag
                <span class="cal-badge" :class="store.estado.calibrated_position ? 'cal-ok' : 'cal-pending'">{{ store.estado.calibrated_position ? '✔' : '⚠' }}</span>
              </button>
              <button
                class="btn btn-calibrar"
                @click="store.calibrar('mpu')"
                :disabled="!store.wsConectado || esOLActivo"
              >
                🔄 MPU
                <span class="cal-badge" :class="store.estado.calibrated_mpu ? 'cal-ok' : 'cal-pending'">{{ store.estado.calibrated_mpu ? '✔' : '⚠' }}</span>
              </button>
            </div>
          </div>

          <!-- PWM slider + input -->
          <div class="ol-param">
            <div class="param-header">
              <span class="control-label">PWM</span>
              <input
                type="number"
                class="param-input"
                v-model.number="pwmVal"
                min="200"
                max="1023"
                step="10"
              />
            </div>
            <input
              type="range"
              class="param-slider"
              v-model.number="pwmVal"
              min="200"
              max="1023"
              step="10"
            />
            <div class="param-range-labels">
              <span>200</span><span>1023</span>
            </div>
          </div>

          <!-- Duration slider + input -->
          <div class="ol-param">
            <div class="param-header">
              <span class="control-label">Duración (s)</span>
              <input
                type="number"
                class="param-input"
                v-model.number="durationVal"
                min="0.1"
                max="3.0"
                step="0.1"
              />
            </div>
            <input
              type="range"
              class="param-slider"
              v-model.number="durationVal"
              min="0.1"
              max="3.0"
              step="0.1"
            />
            <div class="param-range-labels">
              <span>0.1s</span><span>3.0s</span>
            </div>
          </div>

          <!-- Dirección del impulso -->
          <div class="ol-direccion">
            <span class="control-label">Dirección</span>
            <div class="direccion-opciones">
              <label class="dir-option" :class="{ active: directionVal === 'auto' }">
                <input type="radio" v-model="directionVal" value="auto" />
                ⬅➡ AUTO
              </label>
              <label class="dir-option" :class="{ active: directionVal === 'forward' }">
                <input type="radio" v-model="directionVal" value="forward" />
                ➡ ADELANTE
              </label>
              <label class="dir-option" :class="{ active: directionVal === 'backward' }">
                <input type="radio" v-model="directionVal" value="backward" />
                ⬅ ATRÁS
              </label>
            </div>
          </div>

          <!-- Acción -->
          <div class="ol-acciones">
            <button
              class="btn btn-enviar"
              @click="enviar"
              :disabled="!puedeEnviarImpulso"
            >
              🚀 ENVIAR IMPULSO
            </button>

            <button
              class="btn btn-detener"
              @click="store.detenerMotor()"
              :disabled="!store.wsConectado"
            >
              ⬛ DETENER
            </button>
          </div>
        </div>
      </div>

      <!-- Indicadores de todos los sensores -->
      <div class="card ol-card-indicadores">
        <div class="card-header">Indicadores</div>
        <div class="card-body">
          <IndicadoresLazoAbierto />
        </div>
      </div>

    </div>

    <!-- ═══════════ FILA CENTRAL: ANIMACIÓN + GRÁFICOS ═══════════ -->
    <div class="ol-middle-row">
      <div class="animacion-wrapper">
        <AnimacionPendulo />
      </div>
      <div class="graficos-wrapper">
        <GraficosTelemetria :graficos-default="['angle_pend', 'pos_mag_cm', 'pwm', 'rpm_ir']" />
      </div>
    </div>

    <!-- ═══════════ FILA INFERIOR: RESULTADOS ═══════════ -->
    <div class="ol-results-row">
      <ResultadosLazoAbierto />
    </div>

  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { usePendulumStore } from '../../stores/pendulumStore'
import AnimacionPendulo from '../AnimacionPendulo.vue'
import GraficosTelemetria from '../GraficosTelemetria.vue'
import IndicadoresLazoAbierto from '../IndicadoresLazoAbierto.vue'
import ResultadosLazoAbierto from '../ResultadosLazoAbierto.vue'

const store = usePendulumStore()

const pwmVal = ref(500)
const durationVal = ref(1.0)
const directionVal = ref('auto')

const ESTADOS_ACTIVOS_OL = ['OL_HOMING_START', 'OL_HOMING_END', 'OL_IMPULSE_RUNNING']

const esOLActivo = computed(() =>
  ESTADOS_ACTIVOS_OL.includes(store.openLoopState.status)
)

const puedeComando = computed(() =>
  store.wsConectado && !esOLActivo.value
)

const puedeEnviarImpulso = computed(() =>
  store.wsConectado &&
  (store.openLoopState.status === 'OL_READY' || store.openLoopState.status === 'OL_STOPPED')
)

function enviar() {
  store.enviarImpulso(pwmVal.value, durationVal.value, directionVal.value)
}

const estadoOLTexto = computed(() => {
  const mapa = {
    OL_IDLE: 'Inactivo',
    OL_HOMING_START: 'Buscando inicio...',
    OL_HOMING_END: 'Buscando fin...',
    OL_READY: 'Listo',
    OL_IMPULSE_RUNNING: 'Impulso activo',
    OL_STOPPED: 'Detenido',
  }
  return mapa[store.openLoopState.status] || store.openLoopState.status
})

const claseBadgeOL = computed(() => {
  const s = store.openLoopState.status
  if (s === 'OL_IDLE') return 'badge-idle'
  if (s === 'OL_READY' || s === 'OL_STOPPED') return 'badge-listo'
  if (s === 'OL_HOMING_START' || s === 'OL_HOMING_END') return 'badge-calibrando'
  if (s === 'OL_IMPULSE_RUNNING') return 'badge-controlando'
  return 'badge-idle'
})
</script>

<style scoped>
.tab-lazo-abierto {
  display: flex;
  flex-direction: column;
  gap: 16px;
  padding: 16px;
  height: 100%;
  overflow-y: auto;
}

/* ── Fila superior: controles + indicadores ── */
.ol-top-row {
  display: flex;
  gap: 16px;
  flex-shrink: 0;
}

.ol-card-controles {
  flex: 1 1 50%;
  min-width: 0;
}

.ol-card-indicadores {
  flex: 1 1 50%;
  min-width: 0;
}

.ol-controles {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.controles-fila {
  display: flex;
  gap: 16px;
  align-items: flex-start;
}

.ol-estado, .ol-posicion {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.ol-homing {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.homing-botones {
  display: flex;
  gap: 8px;
}

.ol-calibracion {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.calibracion-botones {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
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

.cal-badge {
  font-size: 0.7rem;
  font-weight: 700;
}

.cal-ok { color: #2E7D32; }
.cal-pending { color: #E65100; }

.ol-direccion {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.direccion-opciones {
  display: flex;
  gap: 6px;
}

.dir-option {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 5px 10px;
  border: 1px solid var(--md-outline);
  border-radius: 8px;
  font-size: 0.75rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
  user-select: none;
}

.dir-option input[type="radio"] {
  display: none;
}

.dir-option.active {
  background: #E3F2FD;
  color: #1565C0;
  border-color: #1565C0;
}

/* ── Parámetros (sliders) ───────────────────── */
.ol-param {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.param-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.param-input {
  width: 72px;
  padding: 4px 6px;
  border: 1px solid var(--md-outline);
  border-radius: 6px;
  font-size: 0.85rem;
  font-weight: 600;
  text-align: center;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
  color: #8C2981;
  background: var(--md-surface-dim);
}

.param-input:focus {
  outline: none;
  border-color: #8C2981;
}

.param-slider {
  width: 100%;
  accent-color: #8C2981;
  height: 6px;
}

.param-range-labels {
  display: flex;
  justify-content: space-between;
  font-size: 0.65rem;
  color: var(--md-on-surface-secondary);
}

/* ── Acciones ───────────────────────────────── */
.ol-acciones {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
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
  white-space: nowrap;
}

.btn:disabled {
  opacity: 0.45;
  cursor: not-allowed;
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

.btn-enviar {
  background: #43A047;
  color: white;
  flex: 1;
}

.btn-enviar:hover:not(:disabled) {
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
.badge-posicion { background: #E3F2FD; color: #1565C0; }

/* ── Control label ──────────────────────────── */
.control-label {
  font-size: 0.72rem;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

/* ── Fila central: animación + gráficos ────── */
.ol-middle-row {
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
.ol-results-row {
  flex-shrink: 0;
}

/* ── Responsive ─────────────────────────────── */
@media (max-width: 900px) {
  .ol-top-row {
    flex-direction: column;
  }
  .ol-middle-row {
    flex-direction: column;
  }
  .controles-fila {
    flex-direction: column;
  }
  .homing-botones {
    flex-direction: column;
  }
}
</style>
