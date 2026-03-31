<template>
  <div class="tab-lazo-abierto">

    <!-- ═══════════ FILA SUPERIOR: CONTROLES + ANIMACIÓN + INDICADORES ═══════════ -->
    <div class="ol-top-row">

      <!-- Controles Lazo Abierto -->
      <div class="card ol-card-controles">
        <div class="card-header">Control Lazo Abierto</div>
        <div class="card-body ol-controles">

          <!-- Grid de controles superiores: Calibración + Posicionamiento + Dirección -->
          <div class="ol-grid-superior">

            <!-- Calibración -->
            <div class="ol-seccion">
              <span class="control-label">Calibración</span>
              <div class="seccion-botones">
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('angle')"
                  :disabled="!store.wsConectado || esOLActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_angle }"
                >
                  🎯 Ángulo
                </button>
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('position')"
                  :disabled="!store.wsConectado || esOLActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_position }"
                >
                  📏 Pos. Mag.
                </button>
                <button
                  class="btn btn-calibrar"
                  @click="store.calibrar('mpu')"
                  :disabled="!store.wsConectado || esOLActivo"
                  :class="{ 'cal-ok': store.estado.calibrated_mpu }"
                >
                  🔄 MPU
                </button>
              </div>
            </div>

            <!-- Posicionamiento -->
            <div class="ol-seccion">
              <span class="control-label">Posicionamiento</span>
              <div class="seccion-botones">
                <button
                  class="btn btn-home"
                  @click="store.homeCarro()"
                  :disabled="!puedeComando"
                >
                  ⬅ Home
                </button>
                <button
                  class="btn btn-end"
                  @click="store.endCarro()"
                  :disabled="!puedeComando"
                >
                  End ➡
                </button>
              </div>
            </div>

            <!-- Dirección -->
            <div class="ol-seccion">
              <span class="control-label">Dirección</span>
              <div class="seccion-botones direccion-btns">
                <button
                  class="btn btn-dir"
                  :class="{ active: directionVal === 'auto' }"
                  @click="directionVal = 'auto'"
                >
                  ⬅➡ Auto
                </button>
                <button
                  class="btn btn-dir"
                  :class="{ active: directionVal === 'forward' }"
                  @click="directionVal = 'forward'"
                >
                  ➡ Adelante
                </button>
                <button
                  class="btn btn-dir"
                  :class="{ active: directionVal === 'backward' }"
                  @click="directionVal = 'backward'"
                >
                  ⬅ Atrás
                </button>
              </div>
            </div>

          </div>

          <!-- Sección Impulso -->
          <div class="ol-seccion-impulso">
            <span class="control-label">Impulso</span>

            <div class="impulso-grid">
              <!-- Sliders de PWM y Duración -->
              <div class="impulso-sliders">
                <!-- PWM -->
                <div class="ol-param">
                  <div class="param-header">
                    <span class="param-label">PWM</span>
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

                <!-- Duración -->
                <div class="ol-param">
                  <div class="param-header">
                    <span class="param-label">Duración (s)</span>
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
              </div>

              <!-- Botones de acción -->
              <div class="impulso-acciones">
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

        </div>
      </div>

      <!-- Animación del Péndulo -->
      <div class="animacion-wrapper">
        <AnimacionPendulo />
      </div>

      <!-- Indicadores de todos los sensores -->
      <div class="card ol-card-indicadores">
        <div class="card-header">Indicadores</div>
        <div class="card-body">
          <IndicadoresTelemetria />
        </div>
      </div>

    </div>

    <!-- ═══════════ FILA CENTRAL: GRÁFICOS 2x2 ═══════════ -->
    <div class="ol-middle-row">
      <GraficosTelemetria 
        :graficos-default="['angulo', 'posicion', 'pwm', 'rpm']" 
        layout="grid"
      />
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
import IndicadoresTelemetria from '../IndicadoresTelemetria.vue'
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

/* ── Fila superior: controles + animación + indicadores ── */
.ol-top-row {
  display: flex;
  gap: 16px;
  flex-shrink: 0;
  align-items: stretch;
}

.ol-card-controles {
  flex: 1 1 35%;
  min-width: 0;
  overflow: visible;
}

.animacion-wrapper {
  flex: 0 0 30%;
  min-width: 280px;
  display: flex;
  align-items: stretch;
}

/* Hacer que AnimacionPendulo ocupe todo el alto */
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

.ol-card-indicadores {
  flex: 1 1 35%;
  min-width: 0;
  overflow: visible;
}

.ol-controles {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

/* ── Grid superior: Calibración + Posicionamiento + Dirección ── */
.ol-grid-superior {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
}

.ol-seccion {
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

/* ── Sección Impulso ── */
.ol-seccion-impulso {
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding-top: 12px;
  border-top: 1px solid var(--md-outline-variant, #E0E0E0);
}

.impulso-grid {
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 12px;
  align-items: start;
}

.impulso-sliders {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.impulso-acciones {
  display: flex;
  flex-direction: column;
  gap: 8px;
  justify-content: flex-start;
  min-width: 140px;
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

/* ── Dirección botones ── */
.direccion-btns {
  display: flex;
  gap: 6px;
}

.btn-dir {
  background: #F5F5F5;
  color: #616161;
  border: 1px solid #E0E0E0;
  padding: 6px 10px;
  font-size: 0.75rem;
}

.btn-dir:hover:not(:disabled) {
  background: #EEEEEE;
}

.btn-dir.active {
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

.param-label {
  font-size: 0.72rem;
  font-weight: 600;
  color: var(--md-on-surface-secondary, #757575);
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

/* ── Control label ──────────────────────────── */
.control-label {
  font-size: 0.72rem;
  font-weight: 600;
  color: var(--md-on-surface-secondary);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

/* ── Fila central: gráficos 2x2 ────────────── */
.ol-middle-row {
  flex: 0 0 auto;
  min-height: 380px;
}

/* ── Fila inferior: resultados ──────────────── */
.ol-results-row {
  flex-shrink: 0;
  margin-top: auto;
}

/* ── Responsive ─────────────────────────────── */
@media (max-width: 1100px) {
  .ol-top-row {
    flex-wrap: wrap;
  }
  .ol-card-controles {
    flex: 1 1 100%;
  }
  .animacion-wrapper {
    flex: 1 1 45%;
  }
  .ol-card-indicadores {
    flex: 1 1 45%;
  }
  .ol-grid-superior {
    flex-direction: row;
  }
  .impulso-sliders {
    flex-direction: row;
    gap: 16px;
  }
  .impulso-sliders .ol-param {
    flex: 1;
  }
}

@media (max-width: 700px) {
  .ol-top-row {
    flex-direction: column;
  }
  .animacion-wrapper,
  .ol-card-indicadores {
    flex: 1 1 100%;
  }
  .impulso-sliders {
    flex-direction: column;
  }
}
</style>
