<template>
  <div class="app-shell">
    <!-- ═══════════ HEADER ═══════════ -->
    <header class="app-header">
      <div class="header-izquierda">
        <button
          v-if="store.experimentoActivo"
          class="btn-volver"
          @click="store.experimentoActivo = null"
        >
          ← Volver
        </button>
        <h1>Péndulo Invertido — HMI</h1>
      </div>
      <div class="header-derecha">
        <PanelConexion
          :latencia="latencia"
          :reconectar-manual="reconectarManual"
        />
        <button
          class="btn-emergency"
          :class="{ 'btn-emergency-sent': emergenciaEnviada }"
          @click="onEmergencia"
        >
          {{ emergenciaEnviada ? '✓ ENVIADO' : '⬛ EMERGENCIA' }}
        </button>
      </div>
    </header>

    <!-- ═══════════ CONTENIDO ═══════════ -->
    <div class="app-body">
      <div class="app-content">
        <PaginaInicio v-if="!store.experimentoActivo" />
        <TabRangoPWM v-else-if="store.experimentoActivo === 'rango_pwm'" />
        <TabLazoAbierto v-else-if="store.experimentoActivo === 'lazo_abierto'" />
        <TabControlPidPosicion v-else-if="store.experimentoActivo === 'pid_posicion'" />
        <TabControlPidAngulo v-else-if="store.experimentoActivo === 'pid_angulo'" />
      </div>
    </div>

    <!-- ═══════════ FOOTER ═══════════ -->
    <footer class="app-footer">
      Creado por: Ing. MsC. (c) Jeisson Harvey Martínez Flórez — Docente | Programa: Ingeniería Mecatrónica | Universidad de Pamplona — Villa del Rosario | Todos los derechos reservados — 2026
    </footer>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { usePendulumStore } from './stores/pendulumStore'
import { useWebSocket } from './composables/useWebSocket'

import PanelConexion from './components/PanelConexion.vue'
import PaginaInicio from './components/PaginaInicio.vue'
import TabRangoPWM from './components/tabs/TabRangoPWM.vue'
import TabLazoAbierto from './components/tabs/TabLazoAbierto.vue'
import TabControlPidPosicion from './components/tabs/TabControlPidPosicion.vue'
import TabControlPidAngulo from './components/tabs/TabControlPidAngulo.vue'

const store = usePendulumStore()
const { latencia, reconectarManual } = useWebSocket()

const emergenciaEnviada = ref(false)
let emergenciaTimer = null

function onEmergencia() {
  store.pararEmergencia()
  emergenciaEnviada.value = true
  if (emergenciaTimer) clearTimeout(emergenciaTimer)
  emergenciaTimer = setTimeout(() => { emergenciaEnviada.value = false }, 2000)
}
</script>

<style scoped>
.app-shell {
  display: flex;
  flex-direction: column;
  height: 100vh;
  overflow: hidden;
}

.app-body {
  display: flex;
  flex: 1;
  overflow: hidden;
}

.app-content {
  flex: 1;
  overflow: auto;
  min-width: 0;
}

.app-footer {
  background: #1B0533;
  color: #9B7DB8;
  text-align: center;
  font-size: 0.62rem;
  padding: 6px 16px;
  line-height: 1.4;
  flex-shrink: 0;
}

.header-derecha {
  display: flex;
  align-items: center;
  gap: 16px;
}

.header-izquierda {
  display: flex;
  align-items: center;
  gap: 12px;
}

.btn-volver {
  border: none;
  background: rgba(255,255,255,0.15);
  color: #FFF;
  padding: 6px 14px;
  border-radius: 8px;
  font-size: 0.82rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s;
  white-space: nowrap;
}

.btn-volver:hover {
  background: rgba(255,255,255,0.25);
}

.btn-emergency-sent {
  background: #43A047 !important;
  animation: none !important;
  box-shadow: none !important;
}
</style>
