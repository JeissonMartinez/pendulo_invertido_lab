/**
 * Store Pinia central — Estado reactivo del péndulo invertido.
 * Recibe datos del WebSocket y expone acciones para enviar comandos.
 */

import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

// Tamaño del ring buffer: ~30 s a 50 Hz
const BUFFER_SIZE = 1500

export const usePendulumStore = defineStore('pendulum', () => {
  // ── Estado de conexión ───────────────────────
  const wsConectado = ref(false)
  const mqttConectado = ref(false)

  // ── Última telemetría recibida ───────────────
  const telemetria = ref({
    angle: 0,
    position: 45.0,
    cart_accel: 0,
    rpm: 0,
    pwm: 0,
    direction: 'DETENIDO',
    ts: 0,
  })

  // ── Estado del sistema ───────────────────────
  const estado = ref({
    state: 'IDLE',
    mode: 'MANUAL',
    limits: { start_switch: false, end_switch: false },
    errors: [],
    wifi_rssi: 0,
    calibrated_angle: false,
    calibrated_position: false,
  })

  // ── Historial de telemetría (ring buffer) ────
  const historial = ref({
    ts: [],
    angle: [],
    position: [],
    pwm: [],
    cart_accel: [],
    rpm: [],
  })

  // ── Navegación multi-experimento ─────────────
  const experimentoActivo = ref('rango_pwm')

  // ── Estado del barrido PWM ───────────────────
  const sweepState = ref({
    currentPwm: 0,
    pwmMin: -1,
    pwmMax: -1,
    recordsCount: 0,
    totalSteps: 1023,
  })
  const sweepResults = ref(null)

  // ── WebSocket send callback ──────────────────
  let _wsSend = null

  function setWsSend(fn) {
    _wsSend = fn
  }

  // ── Procesamiento de mensajes entrantes ──────
  // ── Mapeo de nombres firmware → frontend ────
  const DIRECTION_MAP = { 1: 'ADELANTE', '-1': 'ATRAS', 0: 'DETENIDO' }

  function _normalizarTelemetria(d) {
    return {
      angle: d.angle ?? 0,
      position: d.position ?? d.posicion ?? 0,
      cart_accel: d.cart_accel ?? 0,
      rpm: d.rpm ?? 0,
      pwm: d.pwm ?? 0,
      direction: typeof d.direction === 'string'
        ? d.direction
        : (DIRECTION_MAP[d.direccion] ?? DIRECTION_MAP[d.direction] ?? 'DETENIDO'),
      ts: d.ts ?? 0,
      // Preservar sweep embebido en telemetría
      sweep: d.sweep ?? null,
    }
  }

  function _normalizarEstado(d) {
    // El firmware envía "estado" en vez de "state", "ls_inicio"/"ls_fin", etc.
    return {
      state: d.state ?? d.estado ?? 'IDLE',
      mode: d.mode ?? d.modo ?? 'MANUAL',
      limits: d.limits ?? {
        start_switch: d.ls_inicio ?? false,
        end_switch: d.ls_fin ?? false,
      },
      errors: d.errors ?? [],
      wifi_rssi: d.wifi_rssi ?? 0,
      calibrated_angle: d.calibrated_angle ?? false,
      calibrated_position: d.calibrated_position ?? false,
    }
  }

  function procesarMensaje(mensaje) {
    const { tipo, datos } = mensaje

    // Si recibimos cualquier dato, el broker MQTT está activo
    mqttConectado.value = true

    if (tipo === 'telemetria') {
      const norm = _normalizarTelemetria(datos)
      telemetria.value = norm
      _agregarAlHistorial(norm)
      // Extraer sweep state desde telemetría si viene
      if (datos.sweep) {
        sweepState.value = {
          currentPwm: datos.sweep.currentPwm ?? datos.sweep.current_pwm ?? 0,
          pwmMin: datos.sweep.pwmMin ?? datos.sweep.pwm_min ?? -1,
          pwmMax: datos.sweep.pwmMax ?? datos.sweep.pwm_max ?? -1,
          recordsCount: datos.sweep.recordsCount ?? datos.sweep.records_count ?? 0,
          totalSteps: datos.sweep.totalSteps ?? datos.sweep.total_steps ?? 1023,
        }
      }
    } else if (tipo === 'estado') {
      estado.value = _normalizarEstado(datos)
    } else if (tipo === 'limites') {
      // Normalizar nombres de limit switches
      estado.value.limits = datos.limits ?? {
        start_switch: datos.start_switch ?? datos.inicio ?? false,
        end_switch: datos.end_switch ?? datos.fin ?? false,
      }
    } else if (tipo === 'sweep_results') {
      // El firmware envía 3 tipos de mensaje: summary, record, done
      if (datos.type === 'summary') {
        sweepResults.value = {
          pwmMin: datos.pwmMin ?? -1,
          pwmMax: datos.pwmMax ?? -1,
          rangoUtil: datos.rangoUtil ?? 0,
          totalRecords: datos.totalRecords ?? 0,
          records: [],
          complete: false,
        }
      } else if (datos.type === 'record' && sweepResults.value) {
        sweepResults.value.records.push(datos)
      } else if (datos.type === 'done' && sweepResults.value) {
        sweepResults.value.complete = true
      }
    }
  }

  function _agregarAlHistorial(datos) {
    const h = historial.value
    const ahora = Date.now()

    h.ts.push(ahora)
    h.angle.push(datos.angle ?? 0)
    h.position.push(datos.position ?? 45.0)
    h.pwm.push(datos.pwm ?? 0)
    h.cart_accel.push(datos.cart_accel ?? 0)
    h.rpm.push(datos.rpm ?? 0)

    // Recortar al tamaño máximo
    if (h.ts.length > BUFFER_SIZE) {
      const exceso = h.ts.length - BUFFER_SIZE
      h.ts.splice(0, exceso)
      h.angle.splice(0, exceso)
      h.position.splice(0, exceso)
      h.pwm.splice(0, exceso)
      h.cart_accel.splice(0, exceso)
      h.rpm.splice(0, exceso)
    }
  }

  // ── Envío de comandos ────────────────────────
  function enviarComando(tipo, datos = {}) {
    if (_wsSend) {
      _wsSend(JSON.stringify({ tipo, datos }))
    }
  }

  function pararEmergencia() {
    enviarComando('cmd_stop', {})
  }

  function cambiarModo(modo, params = {}) {
    enviarComando('cmd_modo', { mode: modo, params })
  }

  function controlarMotor(action, pwm = 0, direction = 'DETENIDO') {
    enviarComando('cmd_motor', { action, pwm, direction })
  }

  function calibrar(tipo) {
    enviarComando('cmd_calibrar', { type: tipo })
  }

  function configurar(param, value) {
    enviarComando('cmd_config', { param, value })
  }

  function iniciarBarrido() {
    sweepResults.value = null
    enviarComando('cmd_sweep', { action: 'start' })
  }

  function detenerBarrido() {
    enviarComando('cmd_stop', {})
  }

  // ── Computed ─────────────────────────────────
  const estadoTexto = computed(() => estado.value.state)
  const modoTexto = computed(() => estado.value.mode)
  const estaCalibracionCompleta = computed(
    () => estado.value.calibrated_angle && estado.value.calibrated_position
  )

  return {
    // Estado
    wsConectado,
    mqttConectado,
    telemetria,
    estado,
    historial,
    // Navegación
    experimentoActivo,
    // Sweep
    sweepState,
    sweepResults,
    // Computed
    estadoTexto,
    modoTexto,
    estaCalibracionCompleta,
    // Actions
    setWsSend,
    procesarMensaje,
    enviarComando,
    pararEmergencia,
    cambiarModo,
    controlarMotor,
    calibrar,
    configurar,
    iniciarBarrido,
    detenerBarrido,
  }
})
