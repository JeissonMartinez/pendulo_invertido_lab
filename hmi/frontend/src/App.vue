<template>
  <div class="app-shell">
    <!-- ═══════════ HEADER ═══════════ -->
    <header class="app-header">
      <h1>Péndulo Invertido — HMI</h1>
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

    <!-- ═══════════ BODY: NAV + CONTENIDO ═══════════ -->
    <div class="app-body">
      <NavSidebar />

      <!-- Contenido dinámico según pestaña activa -->
      <div class="app-content">
        <TabRangoPWM v-if="store.experimentoActivo === 'rango_pwm'" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { usePendulumStore } from './stores/pendulumStore'
import { useWebSocket } from './composables/useWebSocket'

import PanelConexion from './components/PanelConexion.vue'
import NavSidebar from './components/NavSidebar.vue'
import TabRangoPWM from './components/tabs/TabRangoPWM.vue'

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
  overflow-y: auto;
  overflow-x: hidden;
}

.header-derecha {
  display: flex;
  align-items: center;
  gap: 16px;
}

.btn-emergency-sent {
  background: #43A047 !important;
  animation: none !important;
  box-shadow: none !important;
}
</style>
