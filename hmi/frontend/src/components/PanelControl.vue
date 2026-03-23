<template>
  <div class="card">
    <div class="card-header">Panel de Control</div>
    <div class="card-body panel-control">

      <!-- Selector de Modo -->
      <div class="control-grupo">
        <label class="control-label">Modo de Control</label>
        <select v-model="modoSeleccionado" class="control-select">
          <option v-for="m in modos" :key="m.value" :value="m.value">
            {{ m.label }}
          </option>
        </select>
        <button
          class="btn btn-primario btn-sm"
          @click="aplicarModo"
          :disabled="store.estadoTexto === 'CONTROLLING'"
        >
          Aplicar Modo
        </button>
      </div>

      <!-- Botones de acción principal -->
      <div class="control-grupo acciones-principales">
        <button
          class="btn btn-iniciar"
          @click="iniciarRutina"
          :disabled="!store.estaCalibracionCompleta || store.estadoTexto === 'CONTROLLING'"
        >
          ▶ INICIAR
        </button>

        <button class="btn btn-emergencia" @click="store.pararEmergencia()">
          ⬛ PARADA EMERGENCIA
        </button>
      </div>

      <!-- Controles manuales (solo modo Manual) -->
      <div v-if="store.modoTexto === 'MANUAL'" class="control-grupo controles-manuales">
        <label class="control-label">Control Manual</label>

        <!-- Slider PWM -->
        <div class="slider-grupo">
          <label class="slider-label">PWM: {{ pwmManual }}</label>
          <input
            type="range"
            min="0"
            max="1023"
            v-model.number="pwmManual"
            class="slider"
          />
        </div>

        <!-- Botones dirección -->
        <div class="direccion-btns">
          <button
            class="btn btn-dir"
            @mousedown="moverAdelante"
            @mouseup="detenerMotor"
            @mouseleave="detenerMotor"
          >
            ← Atrás
          </button>
          <button class="btn btn-freno" @click="detenerMotor">
            ■ Freno
          </button>
          <button
            class="btn btn-dir"
            @mousedown="moverAtras"
            @mouseup="detenerMotor"
            @mouseleave="detenerMotor"
          >
            Adelante →
          </button>
        </div>
      </div>

    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const modos = [
  { value: 'MANUAL', label: 'Manual / Lazo Abierto' },
  { value: 'PID', label: 'PID Cascada' },
  { value: 'NEURAL', label: 'Red Neuronal FF' },
  { value: 'SMC', label: 'Régimen Deslizante (SMC)' },
  { value: 'LQR', label: 'LQR' },
  { value: 'FUZZY', label: 'Control Difuso' },
  { value: 'MRAC', label: 'Adaptativo (MRAC)' },
  { value: 'MPC', label: 'MPC' },
]

const modoSeleccionado = ref('MANUAL')
const pwmManual = ref(300)

function aplicarModo() {
  store.cambiarModo(modoSeleccionado.value)
}

function iniciarRutina() {
  store.enviarComando('cmd_modo', {
    mode: modoSeleccionado.value,
    params: { action: 'start' },
  })
}

function moverAdelante() {
  store.controlarMotor('run', pwmManual.value, 'ADELANTE')
}

function moverAtras() {
  store.controlarMotor('run', pwmManual.value, 'ATRAS')
}

function detenerMotor() {
  store.controlarMotor('stop', 0, 'DETENIDO')
}
</script>

<style scoped>
.panel-control {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.control-grupo {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.control-label {
  font-size: 0.75rem;
  font-weight: 600;
  color: #757575;
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.control-select {
  padding: 8px 12px;
  border: 1px solid #E0E0E0;
  border-radius: 6px;
  font-size: 0.9rem;
  background: white;
  cursor: pointer;
}

.control-select:focus {
  outline: none;
  border-color: #1976D2;
  box-shadow: 0 0 0 2px rgba(25, 118, 210, 0.15);
}

/* Botones principales */
.acciones-principales {
  gap: 10px;
}

.btn {
  border: none;
  border-radius: 8px;
  padding: 10px 16px;
  font-size: 0.9rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
  text-align: center;
}

.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-primario {
  background: #1976D2;
  color: white;
}

.btn-primario:hover:not(:disabled) {
  background: #1565C0;
}

.btn-sm {
  padding: 6px 12px;
  font-size: 0.8rem;
}

.btn-iniciar {
  background: #4CAF50;
  color: white;
  font-size: 1rem;
  padding: 12px;
}

.btn-iniciar:hover:not(:disabled) {
  background: #43A047;
}

.btn-emergencia {
  background: #D32F2F;
  color: white;
  font-size: 1rem;
  padding: 14px;
  letter-spacing: 0.05em;
  box-shadow: 0 2px 8px rgba(211, 47, 47, 0.3);
}

.btn-emergencia:hover {
  background: #C62828;
  box-shadow: 0 4px 12px rgba(211, 47, 47, 0.4);
}

/* Controles manuales */
.controles-manuales {
  padding: 12px;
  background: #FFF3E0;
  border-radius: 8px;
  border: 1px solid #FFE0B2;
}

.slider-grupo {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.slider-label {
  font-size: 0.8rem;
  color: #616161;
}

.slider {
  width: 100%;
  accent-color: #1976D2;
}

.direccion-btns {
  display: grid;
  grid-template-columns: 1fr auto 1fr;
  gap: 8px;
}

.btn-dir {
  background: #E3F2FD;
  color: #1565C0;
  padding: 10px 8px;
  font-size: 0.82rem;
}

.btn-dir:hover { background: #BBDEFB; }
.btn-dir:active { background: #90CAF9; }

.btn-freno {
  background: #FFCDD2;
  color: #C62828;
  padding: 10px 12px;
  font-size: 0.82rem;
}

.btn-freno:hover { background: #EF9A9A; }
</style>
