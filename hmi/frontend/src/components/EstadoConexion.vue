<template>
  <div class="estado-conexion">
    <div class="conexion-item" :class="store.wsConectado ? 'con-ok' : 'con-error'">
      <span class="con-led"></span>
      <span class="con-texto">
        {{ store.wsConectado ? 'Conectado' : 'Desconectado' }}
      </span>
    </div>

    <div v-if="store.wsConectado" class="latencia-badge">
      {{ latencia }}ms
    </div>

    <button
      v-if="!store.wsConectado"
      class="btn-reconectar"
      @click="reconectarManual"
    >
      ⟳ Reconectar
    </button>
  </div>
</template>

<script setup>
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const props = defineProps({
  latencia: { type: Number, default: 0 },
  reconectarManual: { type: Function, default: () => {} },
})
</script>

<style scoped>
.estado-conexion {
  display: flex;
  align-items: center;
  gap: 10px;
}

.conexion-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 10px;
  border-radius: 12px;
  font-size: 0.78rem;
  font-weight: 500;
}

.con-ok {
  background: #E8F5E9;
  color: #2E7D32;
}

.con-error {
  background: #FFEBEE;
  color: #C62828;
}

.con-led {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}

.con-ok .con-led { background: #4CAF50; }
.con-error .con-led {
  background: #F44336;
  animation: parpadeo 1.2s ease-in-out infinite;
}

@keyframes parpadeo {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}

.latencia-badge {
  font-size: 0.7rem;
  padding: 2px 6px;
  background: #F5F5F5;
  border-radius: 8px;
  color: #757575;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
}

.btn-reconectar {
  border: none;
  background: #E3F2FD;
  color: #1565C0;
  padding: 4px 10px;
  border-radius: 8px;
  font-size: 0.78rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s;
}

.btn-reconectar:hover {
  background: #BBDEFB;
}
</style>
