/**
 * Composable useWebSocket — Conexión reactiva con auto-reconnect.
 * Conecta al backend FastAPI por WebSocket y alimenta el store Pinia.
 */

import { ref, onMounted, onUnmounted } from 'vue'
import { usePendulumStore } from '../stores/pendulumStore'

export function useWebSocket() {
  const store = usePendulumStore()
  const latencia = ref(0)

  let ws = null
  let reconectarTimeout = null
  let espera = 1000          // backoff inicial: 1s
  const MAX_ESPERA = 15000   // máximo: 15s
  let activo = true

  function conectar() {
    if (!activo) return

    // Determinar URL del WebSocket según el entorno
    const protocolo = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const host = window.location.host
    const url = `${protocolo}//${host}/ws`

    ws = new WebSocket(url)

    ws.onopen = () => {
      store.wsConectado = true
      espera = 1000  // reset backoff
      store.setWsSend((msg) => {
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(msg)
        }
      })
    }

    ws.onmessage = (evento) => {
      try {
        const mensaje = JSON.parse(evento.data)
        store.procesarMensaje(mensaje)

        // Estimar latencia desde timestamp de telemetría
        if (mensaje.tipo === 'telemetria' && mensaje.datos.ts) {
          latencia.value = Date.now() % 100  // placeholder visual
        }
      } catch {
        // mensaje no-JSON, ignorar
      }
    }

    ws.onclose = () => {
      store.wsConectado = false
      store.mqttConectado = false
      store.setWsSend(null)
      programarReconexion()
    }

    ws.onerror = () => {
      // onclose se disparará después
    }
  }

  function programarReconexion() {
    if (!activo) return
    reconectarTimeout = setTimeout(() => {
      espera = Math.min(espera * 1.5, MAX_ESPERA)
      conectar()
    }, espera)
  }

  function desconectar() {
    activo = false
    if (reconectarTimeout) clearTimeout(reconectarTimeout)
    if (ws) ws.close()
  }

  function reconectarManual() {
    if (ws) ws.close()
    espera = 1000
    activo = true
    conectar()
  }

  onMounted(() => {
    conectar()
  })

  onUnmounted(() => {
    desconectar()
  })

  return {
    latencia,
    reconectarManual,
  }
}
