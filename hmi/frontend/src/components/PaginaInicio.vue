<template>
  <div class="pagina-inicio">

    <!-- Encabezado -->
    <div class="inicio-header">
      <div class="inicio-logo">🔬</div>
      <h2 class="inicio-titulo">Péndulo Invertido — HMI</h2>
      <p class="inicio-subtitulo">Seleccione un experimento para comenzar</p>
    </div>

    <!-- Grid de experimentos -->
    <div class="experimentos-grid">

      <button
        v-for="exp in experimentos"
        :key="exp.id"
        class="exp-card"
        @click="store.experimentoActivo = exp.id"
      >
        <span class="exp-icon">{{ exp.icon }}</span>
        <h3 class="exp-titulo">{{ exp.titulo }}</h3>
        <p class="exp-desc">{{ exp.desc }}</p>
        <span class="exp-arrow">→</span>
      </button>

    </div>

    <!-- Pie -->
    <p class="inicio-footer-text">
      Universidad de Pamplona — Ingeniería Mecatrónica
    </p>

  </div>
</template>

<script setup>
import { usePendulumStore } from '../stores/pendulumStore'

const store = usePendulumStore()

const experimentos = [
  {
    id: 'rango_pwm',
    icon: '⚡',
    titulo: 'Rango Operativo PWM',
    desc: 'Barrido automático para detectar la zona muerta y el rango operativo del motor DC.',
  },
  {
    id: 'lazo_abierto',
    icon: '🔓',
    titulo: 'Lazo Abierto',
    desc: 'Control de impulso manual con PWM y duración configurable. Ideal para identificación del sistema.',
  },
  {
    id: 'pid_posicion',
    icon: '🎯',
    titulo: 'Control PID Posición',
    desc: 'Control en lazo cerrado con sintonización PID en tiempo real para posicionamiento del carro.',
  },
]
</script>

<style scoped>
.pagina-inicio {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 100%;
  padding: 48px 24px;
  gap: 40px;
}

/* ── Encabezado ── */
.inicio-header {
  text-align: center;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
}

.inicio-logo {
  font-size: 3rem;
  margin-bottom: 8px;
}

.inicio-titulo {
  font-size: 1.6rem;
  font-weight: 600;
  color: var(--md-on-surface, #1C1019);
  letter-spacing: 0.01em;
  margin: 0;
}

.inicio-subtitulo {
  font-size: 0.95rem;
  color: var(--md-on-surface-secondary, #6E5F72);
  margin: 0;
}

/* ── Grid de experimentos ── */
.experimentos-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 24px;
  width: 100%;
  max-width: 960px;
}

/* ── Card de experimento ── */
.exp-card {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 12px;
  padding: 28px 24px;
  background: var(--md-surface, #FFFFFF);
  border: 1px solid var(--md-outline, #DDD8E0);
  border-radius: 16px;
  box-shadow: var(--shadow-1, 0 1px 3px rgba(28,16,25,0.12));
  cursor: pointer;
  transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  text-align: left;
  position: relative;
  font-family: inherit;
  color: inherit;
}

.exp-card:hover {
  box-shadow: var(--shadow-3, 0 10px 20px rgba(28,16,25,0.15));
  border-color: var(--md-primary, #B5367A);
  transform: translateY(-4px);
}

.exp-card:active {
  transform: translateY(-1px);
  box-shadow: var(--shadow-2, 0 3px 6px rgba(28,16,25,0.15));
}

.exp-icon {
  font-size: 2rem;
}

.exp-titulo {
  font-size: 1.1rem;
  font-weight: 600;
  color: var(--md-on-surface, #1C1019);
  margin: 0;
}

.exp-desc {
  font-size: 0.85rem;
  color: var(--md-on-surface-secondary, #6E5F72);
  line-height: 1.5;
  margin: 0;
  flex: 1;
}

.exp-arrow {
  font-size: 1.2rem;
  color: var(--md-primary, #B5367A);
  font-weight: 600;
  align-self: flex-end;
  opacity: 0;
  transition: opacity 0.2s, transform 0.2s;
  transform: translateX(-8px);
}

.exp-card:hover .exp-arrow {
  opacity: 1;
  transform: translateX(0);
}

/* ── Footer ── */
.inicio-footer-text {
  font-size: 0.75rem;
  color: var(--md-on-surface-secondary, #6E5F72);
  margin: 0;
  opacity: 0.7;
}

/* ── Responsive ── */
@media (max-width: 700px) {
  .pagina-inicio {
    padding: 32px 16px;
    gap: 28px;
  }
  .experimentos-grid {
    grid-template-columns: 1fr;
  }
  .inicio-titulo {
    font-size: 1.3rem;
  }
}
</style>
