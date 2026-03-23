<template>
  <div class="card">
    <div class="card-header">Indicadores de Estado</div>
    <div class="card-body indicadores-grid">

      <!-- Estado del sistema -->
      <div class="indicador">
        <span class="indicador-label">Estado</span>
        <span class="badge" :class="claseBadgeEstado">{{ store.estadoTexto }}</span>
      </div>

      <!-- Modo activo -->
      <div class="indicador">
        <span class="indicador-label">Modo</span>
        <span class="badge badge-modo">{{ store.modoTexto }}</span>
      </div>

      <!-- Ángulo actual -->
      <div class="indicador indicador-grande">
        <span class="indicador-label">Ángulo</span>
        <span class="indicador-valor">
          {{ store.telemetria.angle.toFixed(1) }}°
        </span>
        <span class="indicador-sub">Setpoint: 180.0°</span>
      </div>

      <!-- Posición actual -->
      <div class="indicador indicador-grande">
        <span class="indicador-label">Posición</span>
        <span class="indicador-valor">
          {{ store.telemetria.position.toFixed(1) }} cm
        </span>
        <span class="indicador-sub">Setpoint: 45.0 cm</span>
      </div>

      <!-- PWM actual -->
      <div class="indicador">
        <span class="indicador-label">PWM</span>
        <span class="indicador-valor indicador-valor-sm">
          {{ store.telemetria.pwm }}
        </span>
        <div class="barra-pwm">
          <div
            class="barra-pwm-fill"
            :style="{ width: (Math.abs(store.telemetria.pwm) / 1023 * 100) + '%' }"
          ></div>
        </div>
      </div>

      <!-- Dirección -->
      <div class="indicador">
        <span class="indicador-label">Dirección</span>
        <span class="badge" :class="claseBadgeDireccion">
          {{ store.telemetria.direction }}
        </span>
      </div>

      <!-- RPM -->
      <div class="indicador">
        <span class="indicador-label">RPM Motor</span>
        <span class="indicador-valor indicador-valor-sm">
          {{ store.telemetria.rpm.toFixed(0) }}
        </span>
      </div>

      <!-- Aceleración carro -->
      <div class="indicador">
        <span class="indicador-label">Acel. Carro</span>
        <span class="indicador-valor indicador-valor-sm">
          {{ store.telemetria.cart_accel.toFixed(2) }} m/s²
        </span>
      </div>

      <!-- Limit Switches -->
      <div class="indicador">
        <span class="indicador-label">Limit Switches</span>
        <div class="ls-row">
          <span class="ls-led" :class="store.estado.limits.start_switch ? 'ls-activo' : 'ls-ok'"></span>
          <span class="ls-texto">Inicio</span>
          <span class="ls-led" :class="store.estado.limits.end_switch ? 'ls-activo' : 'ls-ok'" style="margin-left: 12px;"></span>
          <span class="ls-texto">Fin</span>
        </div>
      </div>

      <!-- WiFi RSSI -->
      <div class="indicador">
        <span class="indicador-label">WiFi</span>
        <div class="wifi-barras">
          <div
            v-for="n in 4"
            :key="n"
            class="wifi-barra"
            :class="{ 'wifi-barra-activa': nivelWifi >= n }"
            :style="{ height: (n * 5 + 4) + 'px' }"
          ></div>
        </div>
        <span class="indicador-sub">{{ store.estado.wifi_rssi }} dBm</span>
      </div>

      <!-- Calibración -->
      <div class="indicador">
        <span class="indicador-label">Calibración</span>
        <div class="calibracion-estado">
          <span class="cal-item" :class="store.estado.calibrated_angle ? 'cal-ok' : 'cal-pendiente'">
            Ángulo {{ store.estado.calibrated_angle ? '✓' : '✗' }}
          </span>
          <span class="cal-item" :class="store.estado.calibrated_position ? 'cal-ok' : 'cal-pendiente'">
            Posición {{ store.estado.calibrated_position ? '✓' : '✗' }}
          </span>
        </div>
      </div>

    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const claseBadgeEstado = computed(() => {
  const mapa = {
    IDLE: 'badge-idle',
    CALIBRATING: 'badge-calibrando',
    READY: 'badge-listo',
    SWING_UP: 'badge-swingup',
    CONTROLLING: 'badge-controlando',
    STOPPING: 'badge-deteniendo',
    EMERGENCY: 'badge-emergencia',
  }
  return mapa[store.estadoTexto] || 'badge-idle'
})

const claseBadgeDireccion = computed(() => {
  const d = store.telemetria.direction
  if (d === 'ADELANTE') return 'badge-adelante'
  if (d === 'ATRAS') return 'badge-atras'
  return 'badge-idle'
})

const nivelWifi = computed(() => {
  const rssi = store.estado.wifi_rssi
  if (rssi >= -50) return 4
  if (rssi >= -60) return 3
  if (rssi >= -70) return 2
  if (rssi >= -80) return 1
  return 0
})
</script>

<style scoped>
.indicadores-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
}

.indicador {
  padding: 10px 12px;
  background: #F5F5F5;
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.indicador-label {
  font-size: 0.72rem;
  color: #757575;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  font-weight: 600;
}

.indicador-grande {
  grid-column: span 2;
  text-align: center;
  background: #E3F2FD;
}

.indicador-valor {
  font-size: 1.8rem;
  font-weight: 700;
  color: #1565C0;
  line-height: 1.1;
}

.indicador-valor-sm {
  font-size: 1.2rem;
}

.indicador-sub {
  font-size: 0.7rem;
  color: #9E9E9E;
}

/* Badges */
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

.badge-idle { background: #E0E0E0; color: #616161; }
.badge-calibrando { background: #FFF3E0; color: #E65100; }
.badge-listo { background: #E8F5E9; color: #2E7D32; }
.badge-swingup { background: #E3F2FD; color: #1565C0; }
.badge-controlando { background: #E8F5E9; color: #2E7D32; }
.badge-deteniendo { background: #FFF3E0; color: #E65100; }
.badge-emergencia {
  background: #FFEBEE;
  color: #C62828;
  animation: pulsar 1s ease-in-out infinite;
}
.badge-modo { background: #EDE7F6; color: #4527A0; }
.badge-adelante { background: #E8F5E9; color: #2E7D32; }
.badge-atras { background: #FFF3E0; color: #E65100; }

@keyframes pulsar {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

/* Barra PWM */
.barra-pwm {
  height: 6px;
  background: #E0E0E0;
  border-radius: 3px;
  overflow: hidden;
  margin-top: 4px;
}

.barra-pwm-fill {
  height: 100%;
  background: #1976D2;
  border-radius: 3px;
  transition: width 0.1s ease;
}

/* Limit switches */
.ls-row {
  display: flex;
  align-items: center;
  gap: 4px;
}

.ls-led {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  display: inline-block;
}

.ls-ok { background: #4CAF50; }
.ls-activo { background: #F44336; animation: pulsar 0.8s infinite; }

.ls-texto { font-size: 0.72rem; color: #616161; }

/* WiFi barras */
.wifi-barras {
  display: flex;
  align-items: flex-end;
  gap: 3px;
  height: 24px;
}

.wifi-barra {
  width: 5px;
  background: #E0E0E0;
  border-radius: 2px;
  transition: background 0.3s;
}

.wifi-barra-activa { background: #4CAF50; }

/* Calibración */
.calibracion-estado {
  display: flex;
  gap: 8px;
}

.cal-item {
  font-size: 0.72rem;
  padding: 2px 6px;
  border-radius: 4px;
}

.cal-ok { background: #E8F5E9; color: #2E7D32; }
.cal-pendiente { background: #FFEBEE; color: #C62828; }
</style>
