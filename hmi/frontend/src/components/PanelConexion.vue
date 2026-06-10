<template>
  <div class="panel-conexion">
    <!-- Estado WebSocket -->
    <div class="conexion-item" :class="store.wsConectado ? 'con-ok' : 'con-error'">
      <span class="con-led"></span>
      <span class="con-texto">WS</span>
    </div>

    <!-- Estado MQTT -->
    <div class="conexion-item" :class="store.mqttConectado ? 'con-ok' : 'con-error'" title="Broker MQTT">
      <span class="con-led"></span>
      <span class="con-texto">MQTT</span>
    </div>

    <!-- Estado ESP32 -->
    <div class="conexion-item" :class="esp32Online ? 'con-ok' : 'con-warn'" :title="esp32Title">
      <span class="con-led"></span>
      <span class="con-texto">ESP32</span>
    </div>

    <!-- Selector de tarjeta -->
    <select v-model="tarjetaSeleccionada" class="selector-tarjeta">
      <option value="esp32-wroom-32">ESP32-WROOM-32</option>
    </select>

    <!-- Botón Conectar/Reconectar -->
    <button
      class="btn-conectar"
      :class="{ conectado: store.wsConectado }"
      @click="toggleConexion"
    >
      {{ store.wsConectado ? '⟳ Reconectar' : '● Conectar' }}
    </button>

    <!-- Latencia -->
    <div v-if="store.wsConectado" class="latencia-badge">
      {{ latencia }}ms
    </div>

    <!-- WiFi RSSI -->
    <div v-if="store.estado.wifi_rssi" class="rssi-badge" :class="rssiClass" :title="'WiFi RSSI: ' + store.estado.wifi_rssi + ' dBm'">
      {{ rssiIcon }} {{ store.estado.wifi_rssi }} dBm
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const props = defineProps({
  latencia: { type: Number, default: 0 },
  reconectarManual: { type: Function, default: () => {} },
})

const tarjetaSeleccionada = ref('esp32-wroom-32')

// Ticker para que el computed de esp32Online se re-evalúe periódicamente
const tick = ref(0)
let tickInterval = null
onMounted(() => { tickInterval = setInterval(() => tick.value++, 2000) })
onUnmounted(() => { clearInterval(tickInterval) })

// ESP32 está "online" si hemos recibido telemetría recientemente
const esp32Online = computed(() => {
  // eslint-disable-next-line no-unused-expressions
  tick.value  // dependencia reactiva para re-evaluar
  const h = store.historial.ts
  if (h.length === 0) return false
  return (Date.now() - h.last) < 5000
})

const esp32Title = computed(() => {
  if (esp32Online.value) return 'ESP32 publicando datos'
  return 'ESP32 sin datos recientes'
})

// WiFi RSSI
const rssiClass = computed(() => {
  const rssi = store.estado.wifi_rssi
  if (rssi >= -50) return 'rssi-excelente'
  if (rssi >= -65) return 'rssi-buena'
  if (rssi >= -80) return 'rssi-regular'
  return 'rssi-debil'
})

const rssiIcon = computed(() => {
  const rssi = store.estado.wifi_rssi
  if (rssi >= -50) return '▂▄▆█'
  if (rssi >= -65) return '▂▄▆_'
  if (rssi >= -80) return '▂▄__'
  return '▂___'
})

function toggleConexion() {
  props.reconectarManual()
}
</script>

<style scoped>
.panel-conexion {
  display: flex;
  align-items: center;
  gap: 8px;
}

.conexion-item {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 3px 8px;
  border-radius: 10px;
  font-size: 0.7rem;
  font-weight: 600;
  letter-spacing: 0.02em;
}

.con-ok {
  background: #E8F5E9;
  color: #2E7D32;
}

.con-error {
  background: #FFEBEE;
  color: #C62828;
}

.con-warn {
  background: #FFF3E0;
  color: #E65100;
}

.con-led {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  display: inline-block;
}

.con-ok .con-led { background: #4CAF50; }
.con-error .con-led { background: #F44336; animation: parpadeo 1.2s ease-in-out infinite; }
.con-warn .con-led { background: #FF9800; }

@keyframes parpadeo {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}

.selector-tarjeta {
  padding: 3px 8px;
  border: 1px solid #E0E0E0;
  border-radius: 6px;
  font-size: 0.72rem;
  background: white;
  cursor: pointer;
  color: #424242;
}

.selector-tarjeta:focus {
  outline: none;
  border-color: #B5367A;
}

.btn-conectar {
  border: none;
  padding: 4px 10px;
  border-radius: 8px;
  font-size: 0.72rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s;
  background: #F3E8F9;
  color: #8C2981;
}

.btn-conectar:hover {
  background: #E8D0F4;
}

.btn-conectar.conectado {
  background: #E8F5E9;
  color: #2E7D32;
}

.latencia-badge {
  font-size: 0.65rem;
  padding: 2px 6px;
  background: #F5F5F5;
  border-radius: 8px;
  color: #757575;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.rssi-badge {
  font-size: 0.62rem;
  padding: 2px 6px;
  border-radius: 8px;
  font-family: 'JetBrains Mono', monospace;
  white-space: nowrap;
}
.rssi-excelente { background: #E8F5E9; color: #2E7D32; }
.rssi-buena     { background: #F1F8E9; color: #558B2F; }
.rssi-regular   { background: #FFF3E0; color: #E65100; }
.rssi-debil     { background: #FFEBEE; color: #C62828; }
</style>
