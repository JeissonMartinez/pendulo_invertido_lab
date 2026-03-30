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
    // Extendidos
    pulsos_ir: 0,
    vueltas_ir: 0,
    rps_ir: 0,
    rpm_ir: 0,
    pos_ir_cm: 0,
    rps_mag: 0,
    rpm_mag: 0,
    pos_mag_cm: 0,
    angle_pend: 0,
    vel_ang_pend: 0,
    mpu_ax: 0,
    mpu_ay: 0,
    mpu_az: 0,
    mpu_gx: 0,
    mpu_gy: 0,
    mpu_gz: 0,
    mpu_pitch: 0,
    mpu_roll: 0,
    ls_start: false,
    ls_end: false,
    modo: '',
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
    calibrated_mpu: false,
  })

  // ── Historial de telemetría (ring buffer) ────
  const historial = ref({
    ts: [],
    angle: [],
    position: [],
    pwm: [],
    cart_accel: [],
    rpm: [],
    // Series extendidas
    rpm_ir: [],
    rps_ir: [],
    pos_ir_cm: [],
    rpm_mag: [],
    pos_mag_cm: [],
    angle_pend: [],
    angle_pend_unwrap: [],
    vel_ang_pend: [],
    mpu_ax: [],
    mpu_ay: [],
    mpu_az: [],
    mpu_pitch: [],
    mpu_roll: [],
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

  // ── Estado Lazo Abierto ──────────────────────
  const openLoopState = ref({
    status: 'OL_IDLE',
    lastPosition: null,   // 'start' | 'end' | null
    pwm: 500,
    duration: 1.0,
  })
  const openLoopResults = ref([])

  // ── WebSocket send callback ──────────────────
  let _wsSend = null
  let _mqttWatchdog = null
  const MQTT_TIMEOUT_MS = 5000  // 5s sin mensajes → MQTT desconectado

  function _resetMqttWatchdog() {
    if (_mqttWatchdog) clearTimeout(_mqttWatchdog)
    _mqttWatchdog = setTimeout(() => {
      mqttConectado.value = false
    }, MQTT_TIMEOUT_MS)
  }

  function setWsSend(fn) {
    _wsSend = fn
  }

  // ── Procesamiento de mensajes entrantes ──────
  // ── Mapeo de nombres firmware → frontend ────
  const DIRECTION_MAP = { 1: 'ADELANTE', '-1': 'ATRAS', 0: 'DETENIDO' }

  function _normalizarTelemetria(d) {
    return {
      angle: d.angle ?? d.angle_pend ?? 0,
      position: d.position ?? d.posicion ?? d.pos_mag_cm ?? 0,
      cart_accel: d.cart_accel ?? d.mpu_ax ?? 0,
      rpm: d.rpm ?? d.rpm_ir ?? 0,
      pwm: d.pwm ?? 0,
      direction: typeof d.direction === 'string'
        ? d.direction
        : (DIRECTION_MAP[d.direccion] ?? DIRECTION_MAP[d.direction] ?? 'DETENIDO'),
      ts: d.ts ?? 0,
      // Extendidos
      pulsos_ir: d.pulsos_ir ?? 0,
      vueltas_ir: d.vueltas_ir ?? 0,
      rps_ir: d.rps_ir ?? 0,
      rpm_ir: d.rpm_ir ?? 0,
      pos_ir_cm: d.pos_ir_cm ?? 0,
      rps_mag: d.rps_mag ?? 0,
      rpm_mag: d.rpm_mag ?? 0,
      pos_mag_cm: d.pos_mag_cm ?? 0,
      angle_pend: d.angle_pend ?? d.angle ?? 0,
      vel_ang_pend: d.vel_ang_pend ?? 0,
      mpu_ax: d.mpu_ax ?? 0,
      mpu_ay: d.mpu_ay ?? 0,
      mpu_az: d.mpu_az ?? 0,
      mpu_gx: d.mpu_gx ?? 0,
      mpu_gy: d.mpu_gy ?? 0,
      mpu_gz: d.mpu_gz ?? 0,
      mpu_pitch: d.mpu_pitch ?? 0,
      mpu_roll: d.mpu_roll ?? 0,
      ls_start: d.ls_start ?? false,
      ls_end: d.ls_end ?? false,
      modo: d.modo ?? '',
      // Preservar sweep embebido en telametría
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
      calibrated_mpu: d.calibrated_mpu ?? false,
    }
  }

  function procesarMensaje(mensaje) {
    const { tipo, datos } = mensaje

    // Si recibimos cualquier dato, el broker MQTT está activo
    mqttConectado.value = true
    _resetMqttWatchdog()

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
      // Actualizar openLoopState si estamos en modo OL
      if (datos.modo === 'OPEN_LOOP' || datos.mode === 'OPEN_LOOP') {
        const st = datos.estado ?? datos.state ?? 'OL_IDLE'
        openLoopState.value.status = st
        if (st === 'OL_READY') {
          // Detectar posición
          const ls = estado.value.limits
          if (ls.start_switch) openLoopState.value.lastPosition = 'start'
          else if (ls.end_switch) openLoopState.value.lastPosition = 'end'
        }
      }
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
    // Series extendidas
    h.rpm_ir.push(datos.rpm_ir ?? 0)
    h.rps_ir.push(datos.rps_ir ?? 0)
    h.pos_ir_cm.push(datos.pos_ir_cm ?? 0)
    h.rpm_mag.push(datos.rpm_mag ?? 0)
    h.pos_mag_cm.push(datos.pos_mag_cm ?? 0)
    h.angle_pend.push(datos.angle_pend ?? 0)

    // Unwrap del ángulo para gráficos sin saltos 360→0
    const rawAngle = datos.angle_pend ?? 0
    const prevArr = h.angle_pend_unwrap
    if (prevArr.length === 0) {
      prevArr.push(rawAngle)
    } else {
      const prev = prevArr[prevArr.length - 1]
      let diff = rawAngle - (((prev % 360) + 360) % 360)
      if (diff > 180) diff -= 360
      if (diff < -180) diff += 360
      prevArr.push(prev + diff)
    }

    h.vel_ang_pend.push(datos.vel_ang_pend ?? 0)
    h.mpu_ax.push(datos.mpu_ax ?? 0)
    h.mpu_ay.push(datos.mpu_ay ?? 0)
    h.mpu_az.push(datos.mpu_az ?? 0)
    h.mpu_pitch.push(datos.mpu_pitch ?? 0)
    h.mpu_roll.push(datos.mpu_roll ?? 0)

    // Recortar al tamaño máximo
    if (h.ts.length > BUFFER_SIZE) {
      const exceso = h.ts.length - BUFFER_SIZE
      for (const key of Object.keys(h)) {
        h[key].splice(0, exceso)
      }
    }

    // Si está en modo OPEN_LOOP, agregar a resultados
    if (datos.modo === 'OPEN_LOOP' || experimentoActivo.value === 'lazo_abierto') {
      openLoopResults.value.push({
        ts: ahora,
        ...datos,
      })
      // Limitar a últimos 10000 registros
      if (openLoopResults.value.length > 10000) {
        openLoopResults.value.splice(0, openLoopResults.value.length - 10000)
      }
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

  // ── Acciones Lazo Abierto ───────────────────
  function enviarComandoOpenLoop(action, pwm = 500, duration = 1.0, direction = 'auto') {
    const datos = { action, pwm, duration }
    if (direction && direction !== 'auto') {
      datos.direction = direction
    }
    enviarComando('cmd_openloop', datos)
  }

  function homeCarro() {
    enviarComandoOpenLoop('home')
  }

  function endCarro() {
    enviarComandoOpenLoop('end')
  }

  function enviarImpulso(pwm, duration, direction = 'auto') {
    enviarComandoOpenLoop('impulse', pwm, duration, direction)
  }

  function detenerMotor() {
    enviarComandoOpenLoop('stop')
  }

  function limpiarResultadosOpenLoop() {
    openLoopResults.value = []
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
    // Open Loop
    openLoopState,
    openLoopResults,
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
    // Open Loop actions
    enviarComandoOpenLoop,
    homeCarro,
    endCarro,
    enviarImpulso,
    detenerMotor,
    limpiarResultadosOpenLoop,
  }
})
