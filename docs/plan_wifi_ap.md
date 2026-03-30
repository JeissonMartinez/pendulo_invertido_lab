# Plan: WiFi Access Point Mode para ESP32

## Resumen Ejecutivo (No Técnico)

**¿Qué vamos a hacer?**  
Cambiar la forma en que la ESP32 se conecta a la red. En lugar de conectarse al WiFi de tu casa (donde compite por señal con otros dispositivos), la ESP32 creará su propia red WiFi privada.

**¿Por qué es importante?**  
Tu WiFi doméstico está sobrecargado y la conexión se cae frecuentemente. Esto causa que el motor del péndulo se detenga por seguridad cada vez que pierde comunicación.

**¿Cuál es el beneficio?**  
- Conexión directa y dedicada entre tu PC y la ESP32
- Sin interferencia de otros dispositivos en la casa
- Latencia mínima (~1ms vs ~20-50ms actual)
- Funciona sin internet (ideal para pruebas)

**Desventaja:**  
Tu PC perderá acceso a internet mientras esté conectada al ESP32.

**Riesgo:**  
Bajo. Es un cambio de configuración simple y reversible.

---

## Análisis Técnico

### Estado Actual

```
┌─────────────┐     WiFi (STA)     ┌─────────────┐
│   ESP32     │ ─────────────────► │   Router    │
│  Péndulo    │                    │  Casa "JM"  │
└─────────────┘                    └──────┬──────┘
                                          │
                                   ┌──────▼──────┐
                                   │     PC      │
                                   │  Mosquitto  │
                                   └─────────────┘

Configuración actual (wifi_mqtt_config.h):
- WIFI_SSID: "JM"
- WIFI_PASSWORD: "Meca1020@"  
- MQTT_BROKER: "192.168.1.3" (IP del PC en red doméstica)
- Modo: WIFI_STA (Station)
```

**Problemas:**
- ESP32 compite por ancho de banda con otros dispositivos
- Router asigna IP dinámica (puede cambiar)
- Distancia al router afecta señal (RSSI variable)
- Congestión de red causa latencia impredecible

### Arquitectura Propuesta

```
┌─────────────┐                    ┌─────────────┐
│   ESP32     │ ◄──── WiFi (AP) ───│     PC      │
│  Péndulo    │      Directa       │  Mosquitto  │
│             │                    │             │
│  AP: "ESP32-Pendulum"            │  Cliente    │
│  IP: 192.168.4.1                 │  IP: 192.168.4.2
└─────────────┘                    └─────────────┘

Nueva configuración:
- WIFI_AP_SSID: "ESP32-Pendulum"
- WIFI_AP_PASSWORD: "pendulo123"
- ESP32 IP fija: 192.168.4.1
- PC obtiene: 192.168.4.2 (DHCP del ESP32)
- MQTT_BROKER: "192.168.4.2" (IP del PC en red AP)
```

---

## Archivos a Modificar

| Archivo | Cambios |
|---------|---------|
| `SPISC/include/wifi_mqtt_config.h` | Añadir constantes AP, cambiar MQTT_BROKER |
| `SPISC/lib/MqttBridge/MqttBridge.h` | Añadir declaración de `_connectWifiAP()` |
| `SPISC/lib/MqttBridge/MqttBridge.cpp` | Implementar `_connectWifiAP()`, modificar `loop()` |
| `hmi/mosquitto/mosquitto.conf` | Configurar listener en todas las interfaces |

---

## Pasos de Implementación

### Fase 1: Configuración WiFi AP

**Paso 1.1: Modificar constantes**

Archivo: `SPISC/include/wifi_mqtt_config.h`

Reemplazar TODO el contenido con:

```cpp
/**
 * Configuración WiFi y MQTT — Proyecto Péndulo Invertido.
 *
 * ⚠ EDITAR con las credenciales de tu red antes de compilar.
 *   Este archivo NO se sube al repo (añádelo a .gitignore).
 */

#ifndef WIFI_MQTT_CONFIG_H
#define WIFI_MQTT_CONFIG_H

#include <IPAddress.h>

// ══════════════════════════════════════════════════
// Modo de conexión WiFi
// ══════════════════════════════════════════════════
// Descomentar UNA de las siguientes líneas:
#define WIFI_MODE_AP       // ESP32 crea red WiFi (recomendado para pruebas)
// #define WIFI_MODE_STA   // ESP32 se conecta a router existente

// ══════════════════════════════════════════════════
// Configuración modo Access Point (AP)
// ══════════════════════════════════════════════════
#ifdef WIFI_MODE_AP
    #define WIFI_AP_SSID       "ESP32-Pendulum"
    #define WIFI_AP_PASSWORD   "pendulo123"    // Mínimo 8 caracteres
    #define WIFI_AP_CHANNEL    6               // Canal WiFi (1-13)
    #define WIFI_AP_MAX_CONN   2               // Máximo clientes
    
    // IP fija de la ESP32 en modo AP
    #define AP_LOCAL_IP        IPAddress(192, 168, 4, 1)
    #define AP_GATEWAY         IPAddress(192, 168, 4, 1)
    #define AP_SUBNET          IPAddress(255, 255, 255, 0)
    
    // El broker Mosquitto corre en el PC que se conecta al AP
    // El PC recibirá IP 192.168.4.2 por DHCP
    #define MQTT_BROKER        "192.168.4.2"
#endif

// ══════════════════════════════════════════════════
// Configuración modo Station (STA) - Backup/Alternativa
// ══════════════════════════════════════════════════
#ifdef WIFI_MODE_STA
    #define WIFI_SSID          "JM"
    #define WIFI_PASSWORD      "Meca1020@"
    #define MQTT_BROKER        "192.168.1.3"
#endif

// ══════════════════════════════════════════════════
// MQTT Común (aplica a ambos modos)
// ══════════════════════════════════════════════════
#define MQTT_PORT          1883
#define MQTT_CLIENT_ID     "esp32-pendulum"

// ── Tópicos MQTT ─────────────────────────────────
// Datos: ESP32 → Backend
#define TOPIC_TELEMETRY    "pendulum/telemetry"
#define TOPIC_STATUS       "pendulum/status"
#define TOPIC_LIMITS       "pendulum/limits"

// Comandos: Backend → ESP32
#define TOPIC_CMD_MOTOR     "pendulum/cmd/motor"
#define TOPIC_CMD_MODE      "pendulum/cmd/mode"
#define TOPIC_CMD_CALIBRATE "pendulum/cmd/calibrate"
#define TOPIC_CMD_CONFIG    "pendulum/cmd/config"
#define TOPIC_CMD_STOP      "pendulum/cmd/stop"
#define TOPIC_CMD_SWEEP     "pendulum/cmd/sweep"
#define TOPIC_CMD_OPENLOOP  "pendulum/cmd/openloop"

// Resultados de barrido
#define TOPIC_SWEEP_RESULTS "pendulum/sweep/results"

#endif // WIFI_MQTT_CONFIG_H
```

### Fase 2: Implementar Conexión AP en MqttBridge

**Paso 2.1: Modificar MqttBridge.h**

Archivo: `SPISC/lib/MqttBridge/MqttBridge.h`

Añadir declaración de método privado después de `_connectWifi()`:

```cpp
private:
    // ... otros miembros privados ...
    
    bool _connectWifi();
    bool _connectWifiAP();   // NUEVO: Modo Access Point
    bool _connectWifiSTA();  // NUEVO: Modo Station (renombrado)
    bool _connectMqtt();
    void _resubscribe();
```

**Paso 2.2: Modificar MqttBridge.cpp**

Archivo: `SPISC/lib/MqttBridge/MqttBridge.cpp`

Reemplazar la función `_connectWifi()` con estas tres funciones:

```cpp
// ── Selector de modo WiFi ────────────────────────
bool MqttBridge::_connectWifi() {
#ifdef WIFI_MODE_AP
    return _connectWifiAP();
#else
    return _connectWifiSTA();
#endif
}

// ── Modo Access Point ────────────────────────────
bool MqttBridge::_connectWifiAP() {
    Serial.println("[MqttBridge] Iniciando modo Access Point...");
    
    // Desconectar cualquier conexión previa
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    
    // Configurar IP estática
    if (!WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET)) {
        Serial.println("[MqttBridge] Error configurando IP del AP");
        return false;
    }
    
    // Iniciar AP
    bool success = WiFi.softAP(
        WIFI_AP_SSID, 
        WIFI_AP_PASSWORD,
        WIFI_AP_CHANNEL,
        false,              // SSID visible (no oculto)
        WIFI_AP_MAX_CONN    // Máximo clientes
    );
    
    if (!success) {
        Serial.println("[MqttBridge] Error al crear Access Point");
        return false;
    }
    
    Serial.println("[MqttBridge] ═══════════════════════════════════");
    Serial.println("[MqttBridge] ACCESS POINT ACTIVO");
    Serial.printf( "[MqttBridge]   SSID: %s\n", WIFI_AP_SSID);
    Serial.printf( "[MqttBridge]   Password: %s\n", WIFI_AP_PASSWORD);
    Serial.printf( "[MqttBridge]   IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf( "[MqttBridge]   Canal: %d\n", WIFI_AP_CHANNEL);
    Serial.println("[MqttBridge] ═══════════════════════════════════");
    Serial.printf( "[MqttBridge] Esperando conexión a MQTT en %s:%d\n", 
                   MQTT_BROKER, MQTT_PORT);
    
    return true;
}

// ── Modo Station (original, renombrado) ──────────
bool MqttBridge::_connectWifiSTA() {
#ifdef WIFI_MODE_STA
    Serial.printf("[MqttBridge] Conectando a WiFi '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            Serial.println("[MqttBridge] Timeout WiFi (15s)");
            return false;
        }
        delay(250);
        Serial.print(".");
    }

    Serial.printf("\n[MqttBridge] WiFi OK — IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
#else
    return false;  // STA no habilitado
#endif
}
```

**Paso 2.3: Modificar función `loop()` en MqttBridge.cpp**

Reemplazar la función `loop()` completa:

```cpp
void MqttBridge::loop() {
#ifdef WIFI_MODE_AP
    // ── Modo AP: verificar si hay clientes conectados ──
    static unsigned long lastClientCheck = 0;
    static uint8_t lastClientCount = 0;
    
    uint8_t clientCount = WiFi.softAPgetStationNum();
    
    // Log cambios en clientes conectados
    if (clientCount != lastClientCount) {
        Serial.printf("[MqttBridge] Clientes AP: %d\n", clientCount);
        lastClientCount = clientCount;
    }
    
    // Si no hay clientes, no tiene sentido intentar MQTT
    if (clientCount == 0) {
        // Mostrar recordatorio cada 10 segundos
        if (millis() - lastClientCheck >= 10000) {
            lastClientCheck = millis();
            Serial.printf("[MqttBridge] Esperando cliente... Conectar a WiFi '%s'\n", 
                          WIFI_AP_SSID);
        }
        return;
    }
    
#else
    // ── Modo STA: verificar conexión al router ──
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MqttBridge] WiFi desconectado, reconectando...");
        _connectWifiSTA();
        return;
    }
#endif

    // ── Verificar/reconectar MQTT (común para ambos modos) ──
    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= _reconnectInterval) {
            _lastReconnectAttempt = now;
            
            Serial.printf("[MqttBridge] Intentando MQTT %s:%d...\n", 
                          MQTT_BROKER, MQTT_PORT);
            
            if (_connectMqtt()) {
                _reconnectInterval = 1000;  // Reset backoff
                Serial.println("[MqttBridge] MQTT conectado!");
            } else {
                // Exponential backoff: 1s → 2s → 4s → ... → 30s max
                _reconnectInterval = min(_reconnectInterval * 2, 30000UL);
                Serial.printf("[MqttBridge] MQTT fallo. Reintento en %lu ms\n", 
                              _reconnectInterval);
            }
        }
    }

    _mqttClient.loop();
}
```

**Paso 2.4: Modificar función `getWifiRssi()` en MqttBridge.cpp**

Reemplazar:

```cpp
int MqttBridge::getWifiRssi() const {
#ifdef WIFI_MODE_AP
    // En modo AP, RSSI no aplica (somos el AP, no un cliente)
    // Retornar indicador basado en clientes conectados
    return WiFi.softAPgetStationNum() > 0 ? -30 : -100;
#else
    return WiFi.RSSI();
#endif
}
```

### Fase 3: Configurar Mosquitto en PC

**Paso 3.1: Modificar configuración de Mosquitto**

Archivo: `hmi/mosquitto/mosquitto.conf`

Asegurar que contiene (añadir o modificar):

```conf
# ══════════════════════════════════════════════════
# Configuración Mosquitto para ESP32 Access Point
# ══════════════════════════════════════════════════

# Escuchar en TODAS las interfaces de red
# Necesario para recibir conexiones desde la red del AP (192.168.4.x)
listener 1883 0.0.0.0

# Permitir conexiones sin autenticación (solo para desarrollo)
allow_anonymous true

# Logging para debugging
log_type all
log_dest stdout

# Persistencia (opcional)
persistence true
persistence_location /mosquitto/data/

# Tamaño máximo de mensaje (1MB)
message_size_limit 1048576
```

---

## Verificación

### Test 1: Verificar creación del AP

1. Flashear el firmware a la ESP32
2. Abrir Serial Monitor (115200 baud)
3. Debe mostrar:
   ```
   [MqttBridge] ═══════════════════════════════════
   [MqttBridge] ACCESS POINT ACTIVO
   [MqttBridge]   SSID: ESP32-Pendulum
   [MqttBridge]   Password: pendulo123
   [MqttBridge]   IP: 192.168.4.1
   [MqttBridge]   Canal: 6
   [MqttBridge] ═══════════════════════════════════
   ```

### Test 2: Conectar PC al AP

**macOS:**
```bash
# Conectar al WiFi del ESP32
networksetup -setairportnetwork en0 "ESP32-Pendulum" "pendulo123"

# Esperar 3 segundos
sleep 3

# Verificar IP asignada (debe ser 192.168.4.x)
ipconfig getifaddr en0

# Ping al ESP32
ping -c 3 192.168.4.1
```

**Windows (PowerShell):**
```powershell
# Conectar manualmente desde el icono de WiFi, luego:
ipconfig
ping 192.168.4.1
```

**Linux:**
```bash
nmcli device wifi connect "ESP32-Pendulum" password "pendulo123"
ip addr show wlan0
ping -c 3 192.168.4.1
```

### Test 3: Verificar MQTT

```bash
# En una terminal, iniciar Mosquitto
mosquitto -c hmi/mosquitto/mosquitto.conf -v

# En otra terminal, suscribirse a todos los tópicos
mosquitto_sub -h localhost -t "pendulum/#" -v

# Debe empezar a recibir telemetría del ESP32:
# pendulum/telemetry {"pulsos_ir":0,"vueltas_ir":0.00,...}
# pendulum/status {"estado":"IDLE",...}
```

### Test 4: Enviar comando

```bash
# Enviar comando de parada
mosquitto_pub -h localhost -t "pendulum/cmd/stop" -m "{}"

# Verificar en Serial Monitor del ESP32 que recibió el comando
```

### Test 5: Medir latencia

```bash
# Instalar herramienta de benchmark (opcional)
# Medir round-trip enviando comando y esperando cambio en telemetría
# Latencia esperada: <10ms (vs 20-50ms en modo STA)
```

---

## Comandos Útiles

### macOS

```bash
# Ver redes WiFi disponibles
/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport -s

# Conectar a red específica
networksetup -setairportnetwork en0 "ESP32-Pendulum" "pendulo123"

# Ver IP actual
ipconfig getifaddr en0

# Desconectar WiFi
networksetup -setairportpower en0 off
networksetup -setairportpower en0 on
```

### Windows

```powershell
# Listar redes
netsh wlan show networks

# Conectar (crear perfil primero desde GUI)
netsh wlan connect name="ESP32-Pendulum"

# Ver configuración
ipconfig /all
```

### Linux

```bash
# Listar redes
nmcli device wifi list

# Conectar
nmcli device wifi connect "ESP32-Pendulum" password "pendulo123"

# Verificar
ip addr show
```

---

## Troubleshooting

| Problema | Causa Probable | Solución |
|----------|----------------|----------|
| No aparece red "ESP32-Pendulum" | AP no inició | Verificar Serial Monitor, reiniciar ESP32 |
| PC no obtiene IP | DHCP lento del ESP32 | Esperar 5-10s, o desconectar/reconectar |
| Ping a 192.168.4.1 falla | Firewall en PC | Deshabilitar firewall temporalmente |
| MQTT no conecta | Mosquitto no escucha en 0.0.0.0 | Verificar `listener 1883 0.0.0.0` en config |
| ESP32 muestra "Esperando cliente..." | PC no conectó al AP | Verificar conexión WiFi en PC |
| Error "MQTT fallo, rc=-2" | Broker no encontrado | Verificar IP del broker, Mosquitto corriendo |
| Latencia alta | Canal WiFi congestionado | Cambiar `WIFI_AP_CHANNEL` a 1 o 11 |

### Códigos de error MQTT

| rc | Significado | Acción |
|----|-------------|--------|
| -4 | Timeout de conexión | Verificar IP del broker |
| -3 | Conexión perdida | Reconexión automática |
| -2 | Fallo al conectar | Broker no accesible |
| -1 | Desconectado | Normal antes de conectar |
| 0 | Conectado | ✓ OK |
| 1-5 | Errores de protocolo | Verificar configuración |

---

## Rollback

Para volver al modo Station (conectarse al router de casa):

1. Editar `SPISC/include/wifi_mqtt_config.h`:
   ```cpp
   // #define WIFI_MODE_AP    // Comentar esta línea
   #define WIFI_MODE_STA      // Descomentar esta línea
   ```

2. Verificar que `WIFI_SSID`, `WIFI_PASSWORD` y `MQTT_BROKER` tengan los valores correctos para tu red doméstica

3. Recompilar y flashear:
   ```bash
   cd SPISC
   pio run -t upload
   ```

---

## Consideraciones de Seguridad

| Aspecto | Estado | Nota |
|---------|--------|------|
| Password WPA2 | ✓ 8+ caracteres | Cumple requisito mínimo |
| Red aislada | ✓ Sin internet | No expone dispositivos |
| MQTT sin auth | ⚠ Solo desarrollo | Para producción, añadir usuario/password |
| Alcance WiFi | ~10-15m | Limitado por hardware ESP32 |

---

## Diagrama de Secuencia

```
┌─────────┐          ┌─────────┐          ┌─────────┐
│  ESP32  │          │   PC    │          │Mosquitto│
└────┬────┘          └────┬────┘          └────┬────┘
     │                    │                    │
     │ Crear AP           │                    │
     │ "ESP32-Pendulum"   │                    │
     │◄───────────────────│                    │
     │                    │                    │
     │    Conectar WiFi   │                    │
     │◄───────────────────│                    │
     │                    │                    │
     │  IP: 192.168.4.2   │  Iniciar broker    │
     │────────────────────►│                    │
     │                    │ listener 0.0.0.0   │
     │                    │───────────────────►│
     │                    │                    │
     │ Conectar MQTT a 192.168.4.2:1883       │
     │────────────────────────────────────────►│
     │                    │                    │
     │ Suscribir pendulum/cmd/#               │
     │────────────────────────────────────────►│
     │                    │                    │
     │ Publicar telemetría cada 200ms         │
     │────────────────────────────────────────►│
     │                    │                    │
     │                    │  Suscribir         │
     │                    │  pendulum/#        │
     │                    │───────────────────►│
     │                    │                    │
     │                    │◄──────────────────│
     │                    │  Telemetría       │
     │                    │                    │
```
