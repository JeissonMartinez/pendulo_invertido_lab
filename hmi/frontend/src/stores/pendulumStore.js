/**
 * Store Pinia central — Estado reactivo del péndulo invertido.
 * Recibe datos del WebSocket y expone acciones para enviar comandos.
 */

import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

// Tamaño del ring buffer: ~60 s a 50 Hz
const BUFFER_SIZE = 6000

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
    // PID Posición
    pid_error: 0,
    pid_integral: 0,
    pid_derivada: 0,
    pid_output: 0,
    pid_output_sat: 0,
    pid_setpoint: 0,
    pid_kp: 0,
    pid_ki: 0,
    pid_kd: 0,
    pid_zona_seg: 0,
    pid_tolerancia: 0,
    pid_sensor_type: 0,
    pid_pwm_min: 0,
    pid_pwm_max: 0,
    pid_state: '',
    // PID Ángulo
    pida_error: 0,
    pida_integral: 0,
    pida_derivada: 0,
    pida_output: 0,
    pida_output_sat: 0,
    pida_setpoint: 0,
    pida_kp: 0,
    pida_ki: 0,
    pida_kd: 0,
    pida_rango_trabajo: 0,
    pida_tolerancia: 0,
    pida_pwm_min: 0,
    pida_pwm_max: 0,
    pida_pos_setpoint: 0,
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
    // PID Posición
    pid_error: [],
    pid_integral: [],
    pid_derivada: [],
    pid_output: [],
    pid_output_sat: [],
    pid_setpoint: [],
    // PID Ángulo
    pida_error: [],
    pida_integral: [],
    pida_derivada: [],
    pida_output: [],
    pida_output_sat: [],
    // PID Posición interno (dentro de PID Ángulo)
    pida_pos_error: [],
    pida_pos_output: [],
  })

  // ── Navegación multi-experimento ─────────────
  const experimentoActivo = ref(null)

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

  // ── Estado PID Posición ──────────────────────
  const pidPosState = ref({
    status: 'PID_IDLE',
    kp: 8.0,
    ki: 0.1,
    kd: 0.5,
    setpoint: 45.0,
    zona_seguridad: 5.0,
    tolerancia: 1.0,
    sensor: 'mag',
    pwm_min: 200,
    pwm_max: 700,
  })
  const pidPosResults = ref([])

  // ── Estado PID Ángulo ────────────────────────
  const pidAngState = ref({
    status: 'PIDA_IDLE',
    kp: 2.0,
    ki: 0.05,
    kd: 0.3,
    rango_trabajo: 10.0,
    tolerancia: 2.0,
    pwm_min: 200,
    pwm_max: 700,
  })
  const pidAngResults = ref([])

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
      // PID Posición
      pid_error: d.pid_error ?? 0,
      pid_integral: d.pid_integral ?? 0,
      pid_derivada: d.pid_derivada ?? 0,
      pid_output: d.pid_output ?? 0,
      pid_output_sat: d.pid_output_sat ?? 0,
      pid_setpoint: d.pid_setpoint ?? 0,
      pid_kp: d.pid_kp ?? 0,
      pid_ki: d.pid_ki ?? 0,
      pid_kd: d.pid_kd ?? 0,
      pid_zona_seg: d.pid_zona_seg ?? 0,
      pid_tolerancia: d.pid_tolerancia ?? 0,
      pid_sensor_type: d.pid_sensor_type ?? 0,
      pid_pwm_min: d.pid_pwm_min ?? 0,
      pid_pwm_max: d.pid_pwm_max ?? 0,
      pid_state: d.ol_state ?? d.pid_state ?? '',
      // PID Ángulo
      pida_error: d.pida_error ?? 0,
      pida_integral: d.pida_integral ?? 0,
      pida_derivada: d.pida_derivada ?? 0,
      pida_output: d.pida_output ?? 0,
      pida_output_sat: d.pida_output_sat ?? 0,
      pida_setpoint: d.pida_setpoint ?? 0,
      pida_kp: d.pida_kp ?? 0,
      pida_ki: d.pida_ki ?? 0,
      pida_kd: d.pida_kd ?? 0,
      pida_rango_trabajo: d.pida_rango_trabajo ?? 0,
      pida_tolerancia: d.pida_tolerancia ?? 0,
      pida_pwm_min: d.pida_pwm_min ?? 0,
      pida_pwm_max: d.pida_pwm_max ?? 0,
      pida_pos_setpoint: d.pida_pos_setpoint ?? 0,
      // PID Posición interno
      pida_pos_error: d.pida_pos_error ?? 0,
      pida_pos_integral: d.pida_pos_integral ?? 0,
      pida_pos_derivada: d.pida_pos_derivada ?? 0,
      pida_pos_output: d.pida_pos_output ?? 0,
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
      // Actualizar pidPosState.status desde telemetría cuando modo es PID_POS
      if (norm.modo === 'PID_POS' || datos.modo === 'PID_POS') {
        const pidSt = datos.ol_state ?? datos.pid_state ?? norm.pid_state ?? 'PID_IDLE'
        pidPosState.value.status = pidSt
        // Sincronizar parámetros desde telemetría si están disponibles
        if (norm.pid_kp > 0) pidPosState.value.kp = norm.pid_kp
        if (norm.pid_ki >= 0) pidPosState.value.ki = norm.pid_ki
        if (norm.pid_kd >= 0) pidPosState.value.kd = norm.pid_kd
        if (norm.pid_setpoint > 0) pidPosState.value.setpoint = norm.pid_setpoint
        if (norm.pid_zona_seg > 0) pidPosState.value.zona_seguridad = norm.pid_zona_seg
        if (norm.pid_tolerancia > 0) pidPosState.value.tolerancia = norm.pid_tolerancia
        pidPosState.value.sensor = norm.pid_sensor_type === 1 ? 'ir' : 'mag'
        if (norm.pid_pwm_min > 0) pidPosState.value.pwm_min = norm.pid_pwm_min
        if (norm.pid_pwm_max > 0) pidPosState.value.pwm_max = norm.pid_pwm_max
      }
      // Actualizar pidAngState desde telemetría cuando modo es PID_ANG
      if (norm.modo === 'PID_ANG' || datos.modo === 'PID_ANG') {
        const pidaSt = datos.ol_state ?? datos.pid_state ?? norm.pid_state ?? 'PIDA_IDLE'
        pidAngState.value.status = pidaSt
        if (norm.pida_kp > 0) pidAngState.value.kp = norm.pida_kp
        if (norm.pida_ki >= 0) pidAngState.value.ki = norm.pida_ki
        if (norm.pida_kd >= 0) pidAngState.value.kd = norm.pida_kd
        if (norm.pida_rango_trabajo > 0) pidAngState.value.rango_trabajo = norm.pida_rango_trabajo
        if (norm.pida_tolerancia > 0) pidAngState.value.tolerancia = norm.pida_tolerancia
        if (norm.pida_pwm_min > 0) pidAngState.value.pwm_min = norm.pida_pwm_min
        if (norm.pida_pwm_max > 0) pidAngState.value.pwm_max = norm.pida_pwm_max
      }
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
      // Actualizar pidPosState si estamos en modo PID_POS
      if (datos.modo === 'PID_POS' || datos.mode === 'PID_POS') {
        const st = datos.estado ?? datos.state ?? 'PID_IDLE'
        pidPosState.value.status = st
      }
      // Actualizar pidAngState si estamos en modo PID_ANG
      if (datos.modo === 'PID_ANG' || datos.mode === 'PID_ANG') {
        const st = datos.estado ?? datos.state ?? 'PIDA_IDLE'
        pidAngState.value.status = st
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
    // PID Posición
    h.pid_error.push(datos.pid_error ?? 0)
    h.pid_integral.push(datos.pid_integral ?? 0)
    h.pid_derivada.push(datos.pid_derivada ?? 0)
    h.pid_output.push(datos.pid_output ?? 0)
    h.pid_output_sat.push(datos.pid_output_sat ?? 0)
    h.pid_setpoint.push(datos.pid_setpoint ?? 0)
    // PID Ángulo
    h.pida_error.push(datos.pida_error ?? 0)
    h.pida_integral.push(datos.pida_integral ?? 0)
    h.pida_derivada.push(datos.pida_derivada ?? 0)
    h.pida_output.push(datos.pida_output ?? 0)
    h.pida_output_sat.push(datos.pida_output_sat ?? 0)
    // PID Posición interno
    h.pida_pos_error.push(datos.pida_pos_error ?? 0)
    h.pida_pos_output.push(datos.pida_pos_output ?? 0)

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
        ...datos,
        ts: ahora,  // Sobrescribir el ts del ESP32 con timestamp real
        state: estado.value.state,
      })
      // Limitar a últimos 10000 registros
      if (openLoopResults.value.length > 10000) {
        openLoopResults.value.splice(0, openLoopResults.value.length - 10000)
      }
    }

    // Si está en modo PID_POS, agregar a resultados PID
    if (datos.modo === 'PID_POS' || experimentoActivo.value === 'pid_posicion') {
      pidPosResults.value.push({
        ...datos,
        ts: ahora,
        state: datos.pid_state || pidPosState.value.status || estado.value.state,
      })
      if (pidPosResults.value.length > 10000) {
        pidPosResults.value.splice(0, pidPosResults.value.length - 10000)
      }
    }

    // Si está en modo PID_ANG, agregar a resultados PID ángulo
    if (datos.modo === 'PID_ANG' || experimentoActivo.value === 'pid_angulo') {
      pidAngResults.value.push({
        ...datos,
        ts: ahora,
        state: datos.pid_state || pidAngState.value.status || estado.value.state,
      })
      if (pidAngResults.value.length > 10000) {
        pidAngResults.value.splice(0, pidAngResults.value.length - 10000)
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

  // ── Acciones PID Posición ────────────────────
  function enviarComandoPidPos(action, params = {}) {
    const datos = { action, ...params }
    enviarComando('cmd_pidpos', datos)
  }

  function homePidPos() {
    enviarComandoPidPos('home')
  }

  function endPidPos() {
    enviarComandoPidPos('end')
  }

  function configurarPidPos(config) {
    // Actualizar estado local
    Object.assign(pidPosState.value, config)
    // Enviar comando de configuración
    enviarComandoPidPos('config', config)
  }

  function ejecutarPidPos() {
    enviarComandoPidPos('run')
  }

  function detenerPidPos() {
    enviarComandoPidPos('stop')
  }

  function resetPidPos() {
    enviarComandoPidPos('reset')
  }

  function limpiarResultadosPidPos() {
    pidPosResults.value = []
  }

  // ── Acciones PID Ángulo ──────────────────────
  function enviarComandoPidAng(action, params = {}) {
    const datos = { action, ...params }
    enviarComando('cmd_pidang', datos)
  }

  function homePidAng() {
    enviarComandoPidAng('home')
  }

  function endPidAng() {
    enviarComandoPidAng('end')
  }

  function configurarPidAng(config) {
    Object.assign(pidAngState.value, config)
    enviarComandoPidAng('config', config)
  }

  function ejecutarPidAng() {
    enviarComandoPidAng('run')
  }

  function detenerPidAng() {
    enviarComandoPidAng('stop')
  }

  function resetPidAng() {
    enviarComandoPidAng('reset')
  }

  function limpiarResultadosPidAng() {
    pidAngResults.value = []
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
    // PID Posición
    pidPosState,
    pidPosResults,
    // PID Ángulo
    pidAngState,
    pidAngResults,
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
    // PID Posición actions
    enviarComandoPidPos,
    homePidPos,
    endPidPos,
    configurarPidPos,
    ejecutarPidPos,
    detenerPidPos,
    resetPidPos,
    limpiarResultadosPidPos,
    // PID Ángulo actions
    enviarComandoPidAng,
    homePidAng,
    endPidAng,
    configurarPidAng,
    ejecutarPidAng,
    detenerPidAng,
    resetPidAng,
    limpiarResultadosPidAng,
  }
})
