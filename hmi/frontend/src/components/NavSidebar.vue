<template>
  <nav class="nav-sidebar" :class="{ expandida: expandida }">
    <!-- Toggle -->
    <button class="nav-toggle" @click="expandida = !expandida" :title="expandida ? 'Colapsar' : 'Expandir'">
      <span class="nav-toggle-icon">{{ expandida ? '◂' : '▸' }}</span>
    </button>

    <!-- Sección: Experimentos -->
    <div class="nav-section">
      <span v-if="expandida" class="nav-section-label">Experimentos</span>
    </div>

    <button
      v-for="tab in tabs"
      :key="tab.id"
      class="nav-item"
      :class="{ activo: store.experimentoActivo === tab.id }"
      @click="store.experimentoActivo = tab.id"
      :title="tab.label"
    >
      <span class="nav-icon">{{ tab.icon }}</span>
      <span v-if="expandida" class="nav-label">{{ tab.label }}</span>
    </button>

    <!-- Spacer -->
    <div class="nav-spacer"></div>

    <!-- Info -->
    <div v-if="expandida" class="nav-footer">
      <span class="nav-footer-text">Péndulo Invertido</span>
      <span class="nav-footer-version">HMI v0.2</span>
    </div>
  </nav>
</template>

<script setup>
import { ref } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()
const expandida = ref(false)

const tabs = [
  { id: 'rango_pwm', label: 'Rango Operativo PWM', icon: '⚡' },
  { id: 'lazo_abierto', label: 'Lazo Abierto', icon: '🔓' },
  // Futuras pestañas:
  // { id: 'control_pid', label: 'Control PID', icon: '🎯' },
  // { id: 'control_neural', label: 'Red Neuronal', icon: '🧠' },
]
</script>

<style scoped>
.nav-sidebar {
  display: flex;
  flex-direction: column;
  background: #1B0533;
  color: #D4BDE0;
  width: 56px;
  min-height: 100%;
  transition: width 0.2s ease;
  overflow: hidden;
  flex-shrink: 0;
}

.nav-sidebar.expandida {
  width: 200px;
}

.nav-toggle {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 44px;
  background: transparent;
  border: none;
  color: #9B7DB8;
  cursor: pointer;
  font-size: 1rem;
  transition: color 0.15s;
}

.nav-toggle:hover {
  color: #E8D0F4;
}

.nav-section {
  padding: 12px 16px 4px;
}

.nav-section-label {
  font-size: 0.65rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #7B5E96;
  font-weight: 600;
}

.nav-item {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 100%;
  padding: 10px 16px;
  background: transparent;
  border: none;
  color: #9B7DB8;
  cursor: pointer;
  font-size: 0.82rem;
  text-align: left;
  transition: all 0.15s;
  white-space: nowrap;
}

.nav-item:hover {
  background: #2D0053;
  color: #E8D0F4;
}

.nav-item.activo {
  background: #B5367A;
  color: #FFFFFF;
  font-weight: 600;
}

.nav-icon {
  font-size: 1.1rem;
  width: 24px;
  text-align: center;
  flex-shrink: 0;
}

.nav-label {
  overflow: hidden;
  text-overflow: ellipsis;
}

.nav-spacer {
  flex: 1;
}

.nav-footer {
  padding: 12px 16px;
  border-top: 1px solid #2D0053;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.nav-footer-text {
  font-size: 0.72rem;
  color: #7B5E96;
  font-weight: 600;
}

.nav-footer-version {
  font-size: 0.65rem;
  color: #5C3F73;
}
</style>
