<template>
  <div class="card">
    <div class="card-header">Calibración</div>
    <div class="card-body panel-calibracion">

      <!-- Botones de calibración -->
      <div class="cal-botones">
        <button
          class="btn btn-cal"
          @click="calibrarAngulo"
          :disabled="calibrando"
        >
          <span class="cal-icono">⊙</span>
          Calibrar Ángulo
        </button>

        <button
          class="btn btn-cal"
          @click="calibrarPosicion"
          :disabled="calibrando"
        >
          <span class="cal-icono">↔</span>
          Calibrar Posición
        </button>

        <button
          class="btn btn-cal btn-cal-completa"
          @click="calibracionCompleta"
          :disabled="calibrando"
        >
          <span class="cal-icono">⟳</span>
          Calibración Completa
        </button>
      </div>

      <!-- Barra de progreso -->
      <div v-if="calibrando" class="cal-progreso">
        <div class="cal-progreso-texto">
          {{ mensajeCalibracion }}
        </div>
        <div class="barra-progreso">
          <div class="barra-progreso-fill" :style="{ width: progreso + '%' }"></div>
        </div>
      </div>

      <!-- Estado de calibración -->
      <div class="cal-resultado">
        <div class="cal-fila">
          <span class="cal-label">Ángulo:</span>
          <span :class="store.estado.calibrated_angle ? 'cal-ok' : 'cal-pendiente'">
            {{ store.estado.calibrated_angle ? '✓ Calibrado' : '✗ Pendiente' }}
          </span>
        </div>
        <div class="cal-fila">
          <span class="cal-label">Posición:</span>
          <span :class="store.estado.calibrated_position ? 'cal-ok' : 'cal-pendiente'">
            {{ store.estado.calibrated_position ? '✓ Calibrado' : '✗ Pendiente' }}
          </span>
        </div>
      </div>

    </div>
  </div>
</template>

<script setup>
import { ref, watch } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const calibrando = ref(false)
const progreso = ref(0)
const mensajeCalibracion = ref('')

let progresoInterval = null

function iniciarProgreso(mensaje, duracion = 5000) {
  calibrando.value = true
  progreso.value = 0
  mensajeCalibracion.value = mensaje

  const paso = 100 / (duracion / 100)
  clearInterval(progresoInterval)
  progresoInterval = setInterval(() => {
    progreso.value = Math.min(progreso.value + paso, 95)
  }, 100)
}

function finalizarProgreso() {
  progreso.value = 100
  clearInterval(progresoInterval)
  setTimeout(() => {
    calibrando.value = false
    progreso.value = 0
  }, 800)
}

function calibrarAngulo() {
  iniciarProgreso('Calibrando ángulo...', 3000)
  store.calibrar('angle')
}

function calibrarPosicion() {
  iniciarProgreso('Calibrando posición... Moviendo carro a límites.', 8000)
  store.calibrar('position')
}

function calibracionCompleta() {
  iniciarProgreso('Calibración completa en progreso...', 12000)
  store.calibrar('full')
}

// Cuando el estado cambia a calibrado, finalizar la barra
watch(
  () => [store.estado.calibrated_angle, store.estado.calibrated_position],
  () => {
    if (calibrando.value) {
      const estadoActual = store.estadoTexto
      if (estadoActual !== 'CALIBRATING') {
        finalizarProgreso()
      }
    }
  }
)

watch(
  () => store.estadoTexto,
  (nuevo) => {
    if (calibrando.value && nuevo !== 'CALIBRATING') {
      finalizarProgreso()
    }
  }
)
</script>

<style scoped>
.panel-calibracion {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.cal-botones {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.btn-cal {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 14px;
  border: 1px solid #E0E0E0;
  border-radius: 8px;
  background: white;
  font-size: 0.85rem;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.15s ease;
  color: #424242;
}

.btn-cal:hover:not(:disabled) {
  background: #E3F2FD;
  border-color: #90CAF9;
}

.btn-cal:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-cal-completa {
  background: #E8F5E9;
  border-color: #A5D6A7;
  font-weight: 600;
}

.btn-cal-completa:hover:not(:disabled) {
  background: #C8E6C9;
}

.cal-icono {
  font-size: 1.1rem;
}

/* Progreso */
.cal-progreso {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.cal-progreso-texto {
  font-size: 0.8rem;
  color: #1565C0;
  font-weight: 500;
}

.barra-progreso {
  height: 6px;
  background: #E0E0E0;
  border-radius: 3px;
  overflow: hidden;
}

.barra-progreso-fill {
  height: 100%;
  background: linear-gradient(90deg, #42A5F5, #1976D2);
  border-radius: 3px;
  transition: width 0.15s ease;
}

/* Resultado */
.cal-resultado {
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding: 10px;
  background: #FAFAFA;
  border-radius: 6px;
}

.cal-fila {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 0.82rem;
}

.cal-label {
  color: #757575;
}

.cal-ok {
  color: #2E7D32;
  font-weight: 600;
}

.cal-pendiente {
  color: #C62828;
  font-weight: 500;
}
</style>
