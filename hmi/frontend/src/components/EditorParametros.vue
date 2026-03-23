<template>
  <div class="card">
    <div class="card-header">Parámetros del Sistema</div>
    <div class="card-body editor-params">

      <!-- Grupo: Seguridad Ángulo -->
      <fieldset class="param-grupo">
        <legend>Ángulo</legend>
        <div class="param-fila">
          <label>MIN_ANGLE (°)</label>
          <input type="number" v-model.number="params.MIN_ANGLE" step="1" />
        </div>
        <div class="param-fila">
          <label>MAX_ANGLE (°)</label>
          <input type="number" v-model.number="params.MAX_ANGLE" step="1" />
        </div>
      </fieldset>

      <!-- Grupo: Posición -->
      <fieldset class="param-grupo">
        <legend>Posición</legend>
        <div class="param-fila">
          <label>MIN_POSITION (cm)</label>
          <input type="number" v-model.number="params.MIN_POSITION" step="0.5" />
        </div>
        <div class="param-fila">
          <label>MAX_POSITION (cm)</label>
          <input type="number" v-model.number="params.MAX_POSITION" step="0.5" />
        </div>
        <div class="param-fila">
          <label>Setpoint (cm)</label>
          <input type="number" v-model.number="params.posicionSetpoint" step="0.5" />
        </div>
        <div class="param-fila">
          <label>ZONA_ALERTA (cm)</label>
          <input type="number" v-model.number="params.ZONA_ALERTA" step="0.5" />
        </div>
      </fieldset>

      <!-- Grupo: Ganancias -->
      <fieldset class="param-grupo">
        <legend>Ganancias</legend>
        <div class="param-fila">
          <label>KP_POS</label>
          <input type="number" v-model.number="params.KP_POS" step="0.01" />
        </div>
        <div v-if="store.modoTexto === 'PID'" class="param-extra">
          <div class="param-fila">
            <label>KP_ANGLE</label>
            <input type="number" v-model.number="params.KP_ANGLE" step="0.1" />
          </div>
          <div class="param-fila">
            <label>KI_ANGLE</label>
            <input type="number" v-model.number="params.KI_ANGLE" step="0.01" />
          </div>
          <div class="param-fila">
            <label>KD_ANGLE</label>
            <input type="number" v-model.number="params.KD_ANGLE" step="0.01" />
          </div>
        </div>
        <div v-if="store.modoTexto === 'NEURAL'" class="param-extra">
          <div class="param-fila">
            <label>Learning Rate</label>
            <input type="number" v-model.number="params.LEARNING_RATE" step="0.01" />
          </div>
        </div>
        <div v-if="store.modoTexto === 'SMC'" class="param-extra">
          <div class="param-fila">
            <label>Lambda (λ)</label>
            <input type="number" v-model.number="params.SMC_LAMBDA" step="0.1" />
          </div>
          <div class="param-fila">
            <label>K (ganancia)</label>
            <input type="number" v-model.number="params.SMC_K" step="0.1" />
          </div>
          <div class="param-fila">
            <label>Capa límite</label>
            <input type="number" v-model.number="params.SMC_BOUNDARY" step="0.01" />
          </div>
        </div>
        <div v-if="store.modoTexto === 'LQR'" class="param-extra">
          <div class="param-fila">
            <label>Q1 (ángulo)</label>
            <input type="number" v-model.number="params.LQR_Q1" step="0.1" />
          </div>
          <div class="param-fila">
            <label>Q2 (vel. angular)</label>
            <input type="number" v-model.number="params.LQR_Q2" step="0.1" />
          </div>
          <div class="param-fila">
            <label>Q3 (posición)</label>
            <input type="number" v-model.number="params.LQR_Q3" step="0.1" />
          </div>
          <div class="param-fila">
            <label>R (control)</label>
            <input type="number" v-model.number="params.LQR_R" step="0.01" />
          </div>
        </div>
      </fieldset>

      <!-- Botones -->
      <div class="param-acciones">
        <button class="btn btn-aplicar" @click="aplicarTodos">
          Aplicar Cambios
        </button>
        <button class="btn btn-restaurar" @click="restaurarDefaults">
          Restaurar Defaults
        </button>
      </div>

      <div v-if="mensajeExito" class="param-mensaje-exito">
        {{ mensajeExito }}
      </div>

    </div>
  </div>
</template>

<script setup>
import { reactive, ref } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const DEFAULTS = {
  MIN_ANGLE: 130,
  MAX_ANGLE: 230,
  MIN_POSITION: 20,
  MAX_POSITION: 80,
  posicionSetpoint: 45,
  ZONA_ALERTA: 10,
  KP_POS: 0.5,
  KP_ANGLE: 8.0,
  KI_ANGLE: 0.1,
  KD_ANGLE: 0.5,
  LEARNING_RATE: 0.4,
  SMC_LAMBDA: 5.0,
  SMC_K: 10.0,
  SMC_BOUNDARY: 0.1,
  LQR_Q1: 100,
  LQR_Q2: 10,
  LQR_Q3: 1,
  LQR_R: 0.01,
}

const params = reactive({ ...DEFAULTS })
const mensajeExito = ref('')

function aplicarTodos() {
  for (const [param, value] of Object.entries(params)) {
    store.configurar(param, value)
  }
  mensajeExito.value = '✓ Parámetros enviados'
  setTimeout(() => { mensajeExito.value = '' }, 2500)
}

function restaurarDefaults() {
  Object.assign(params, DEFAULTS)
  mensajeExito.value = '↺ Defaults restaurados (no enviados aún)'
  setTimeout(() => { mensajeExito.value = '' }, 2500)
}
</script>

<style scoped>
.editor-params {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.param-grupo {
  border: 1px solid #E0E0E0;
  border-radius: 8px;
  padding: 12px;
  margin: 0;
}

.param-grupo legend {
  font-size: 0.75rem;
  font-weight: 600;
  color: #1565C0;
  text-transform: uppercase;
  padding: 0 6px;
  letter-spacing: 0.04em;
}

.param-fila {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 4px 0;
  gap: 8px;
}

.param-fila label {
  font-size: 0.8rem;
  color: #616161;
  white-space: nowrap;
}

.param-fila input {
  width: 90px;
  padding: 5px 8px;
  border: 1px solid #E0E0E0;
  border-radius: 6px;
  text-align: right;
  font-size: 0.85rem;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.param-fila input:focus {
  outline: none;
  border-color: #1976D2;
  box-shadow: 0 0 0 2px rgba(25, 118, 210, 0.15);
}

.param-extra {
  margin-top: 4px;
  padding-top: 6px;
  border-top: 1px dashed #E0E0E0;
}

.param-acciones {
  display: flex;
  gap: 8px;
}

.btn {
  border: none;
  border-radius: 8px;
  padding: 8px 14px;
  font-size: 0.82rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
  flex: 1;
}

.btn-aplicar {
  background: #1976D2;
  color: white;
}

.btn-aplicar:hover { background: #1565C0; }

.btn-restaurar {
  background: #F5F5F5;
  color: #616161;
  border: 1px solid #E0E0E0;
}

.btn-restaurar:hover { background: #EEEEEE; }

.param-mensaje-exito {
  font-size: 0.8rem;
  color: #2E7D32;
  font-weight: 500;
  text-align: center;
  padding: 6px;
  background: #E8F5E9;
  border-radius: 6px;
}
</style>
