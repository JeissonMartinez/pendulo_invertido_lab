```zsh
admin@MacBook-Pro-de-Admin SPISC % ~/.platformio/penv/bin/pio device monitor -b 115200 
--- forcing DTR inactive
--- forcing RTS inactive
--- Terminal on /dev/cu.wchusbserial110 | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
[  7092][E][WiFiClient.cpp:275] connect(): socket error on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] MQTT fallo, rc=-2
[MqttBridge] MQTT fallo. Reintento en 4000 ms
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[ 11086][E][WiFiClient.cpp:275] connect(): socket error on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] MQTT fallo, rc=-2
[MqttBridge] MQTT fallo. Reintento en 5000 ms
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[ 16044][E][WiFiClient.cpp:275] connect(): socket error on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] MQTT fallo, rc=-2
[MqttBridge] MQTT fallo. Reintento en 5000 ms
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[ 21121][E][WiFiClient.cpp:275] connect(): socket error on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] MQTT fallo, rc=-2
[MqttBridge] MQTT fallo. Reintento en 5000 ms
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 20:21:17.224
[TimeSync] Sincronizado con servidor: 20:21:27.225
[TimeSync] Sincronizado con servidor: 20:21:37.228
[TimeSync] Sincronizado con servidor: 20:21:47.228
[TimeSync] Sincronizado con servidor: 20:21:57.230
[sensors] Ángulo calibrado. Offset: 151.9° (0°=arriba, 180°=abajo)
[TimeSync] Sincronizado con servidor: 20:22:07.232
[sensors] Posición magnética calibrada a 0.0 cm
[main] Cambio de modo → PID_POS
[pidpos] Modo PID Posición activado.
[pidpos] Ya en home, listo para configurar.
[pidpos] Homing a FIN...
[TimeSync] Sincronizado con servidor: 20:22:17.233
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=66)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[TimeSync] Sincronizado con servidor: 20:22:27.234
[TimeSync] Sincronizado con servidor: 20:22:37.235
[TimeSync] Sincronizado con servidor: 20:22:47.236
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=45.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=MAG PWM=[300,700]
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=45.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:22:55.943][pidpos] Iniciando control PID → SP=45.0 cm
[20:22:57.031][pidpos] Setpoint alcanzado (pos=44.94, SP=45.00, err=0.062)
[TimeSync] Sincronizado con servidor: 20:22:57.237
[TimeSync] Sincronizado con servidor: 20:23:07.238
[TimeSync] Sincronizado con servidor: 20:23:17.239
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=200)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[20:23:21.637][pidpos] Iniciando control PID → SP=45.0 cm
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=467)
[main] Auto-calibración: LS_START → posición = 0 cm
[20:23:22.783][pidpos] Setpoint alcanzado (pos=44.74, SP=45.00, err=0.261)
[safety] Timeout MQTT — frenando motor
[TimeSync] Sincronizado con servidor: 20:23:27.241
[TimeSync] Sincronizado con servidor: 20:23:37.242
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=75)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[20:23:45.047][pidpos] Iniciando control PID → SP=45.0 cm
[20:23:46.123][pidpos] Setpoint alcanzado (pos=44.74, SP=45.00, err=0.261)
[TimeSync] Sincronizado con servidor: 20:23:47.243
[TimeSync] Sincronizado con servidor: 20:23:57.243
[TimeSync] Sincronizado con servidor: 20:24:07.245
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=4152)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[20:24:16.340][pidpos] Iniciando control PID → SP=45.0 cm
[TimeSync] Sincronizado con servidor: 20:24:17.247
[20:24:17.411][pidpos] Setpoint alcanzado (pos=44.74, SP=45.00, err=0.261)
[TimeSync] Sincronizado con servidor: 20:24:27.249
[TimeSync] Sincronizado con servidor: 20:24:37.250
[TimeSync] Sincronizado con servidor: 20:24:47.252
[TimeSync] Sincronizado con servidor: 20:24:57.253
[TimeSync] Sincronizado con servidor: 20:25:07.255
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=8401)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[TimeSync] Sincronizado con servidor: 20:25:17.256
[20:25:18.519][pidpos] Iniciando control PID → SP=45.0 cm
[20:25:19.674][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[TimeSync] Sincronizado con servidor: 20:25:27.257
[pidpos] Homing a INICIO...
[pidpos] Home (inicio) alcanzado.
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=8389)
[main] Auto-calibración: LS_START → posición = 0 cm
[TimeSync] Sincronizado con servidor: 20:25:37.258
[TimeSync] Sincronizado con servidor: 20:25:47.260
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=20.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 20:25:57.262
[20:25:57.376][pidpos] Iniciando control PID → SP=20.0 cm
[20:25:58.435][pidpos] Setpoint alcanzado (pos=19.74, SP=20.00, err=0.259)
[TimeSync] Sincronizado con servidor: 20:26:07.264
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=29.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:26:14.395][pidpos] Iniciando control PID → SP=29.0 cm
[20:26:15.455][pidpos] Setpoint alcanzado (pos=28.77, SP=29.00, err=0.232)
[TimeSync] Sincronizado con servidor: 20:26:17.265
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=40.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:26:24.392][pidpos] Iniciando control PID → SP=40.0 cm
[20:26:25.516][pidpos] Setpoint alcanzado (pos=39.78, SP=40.00, err=0.221)
[TimeSync] Sincronizado con servidor: 20:26:27.267
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=50.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:26:32.456][pidpos] Iniciando control PID → SP=50.0 cm
[20:26:33.478][pidpos] Setpoint alcanzado (pos=49.60, SP=50.00, err=0.400)
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=60.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 20:26:37.269
[20:26:38.467][pidpos] Iniciando control PID → SP=60.0 cm
[20:26:39.525][pidpos] Setpoint alcanzado (pos=59.72, SP=60.00, err=0.282)
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=70.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 20:26:47.270
[20:26:47.289][pidpos] Iniciando control PID → SP=70.0 cm
[20:26:48.381][pidpos] Setpoint alcanzado (pos=69.74, SP=70.00, err=0.262)
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=50.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:26:55.051][pidpos] Iniciando control PID → SP=50.0 cm
[20:26:56.033][pidpos] Setpoint alcanzado (pos=50.29, SP=50.00, err=-0.294)
[TimeSync] Sincronizado con servidor: 20:26:57.272
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=30.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 20:27:07.273
[20:27:08.356][pidpos] Iniciando control PID → SP=30.0 cm
[20:27:09.345][pidpos] Setpoint alcanzado (pos=30.16, SP=30.00, err=-0.157)
[TimeSync] Sincronizado con servidor: 20:27:17.274
[TimeSync] Sincronizado con servidor: 20:27:27.276
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=66.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 20:27:37.278
[20:27:37.640][pidpos] Iniciando control PID → SP=66.0 cm
[20:27:38.762][pidpos] Setpoint alcanzado (pos=65.77, SP=66.00, err=0.230)
[TimeSync] Sincronizado con servidor: 20:27:47.279
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=56.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:27:53.363][pidpos] Iniciando control PID → SP=56.0 cm
[20:27:54.271][pidpos] Setpoint alcanzado (pos=56.25, SP=56.00, err=-0.246)
[TimeSync] Sincronizado con servidor: 20:27:57.280
[20:27:58.455][pidpos] Iniciando control PID → SP=56.0 cm
[20:27:58.959][pidpos] Setpoint alcanzado (pos=56.25, SP=56.00, err=-0.246)
[TimeSync] Sincronizado con servidor: 20:28:07.282
[TimeSync] Sincronizado con servidor: 20:28:17.283
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=66.5 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:28:21.735][pidpos] Iniciando control PID → SP=66.5 cm
[20:28:22.796][pidpos] Setpoint alcanzado (pos=66.07, SP=66.50, err=0.433)
[TimeSync] Sincronizado con servidor: 20:28:27.284
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=50.5 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 20:28:37.286
[20:28:41.366][pidpos] Iniciando control PID → SP=50.5 cm
[20:28:42.371][pidpos] Setpoint alcanzado (pos=50.69, SP=50.50, err=-0.191)
[TimeSync] Sincronizado con servidor: 20:28:47.286
[TimeSync] Sincronizado con servidor: 20:28:57.288
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=36.5 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:29:05.107][pidpos] Iniciando control PID → SP=36.5 cm
[20:29:06.083][pidpos] Setpoint alcanzado (pos=36.60, SP=36.50, err=-0.105)
[TimeSync] Sincronizado con servidor: 20:29:07.290
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=27.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:29:14.314][pidpos] Iniciando control PID → SP=27.0 cm
[20:29:15.278][pidpos] Setpoint alcanzado (pos=27.08, SP=27.00, err=-0.082)
[TimeSync] Sincronizado con servidor: 20:29:17.291
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=16.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:29:21.795][pidpos] Iniciando control PID → SP=16.0 cm
[20:29:22.812][pidpos] Setpoint alcanzado (pos=15.87, SP=16.00, err=0.128)
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=5.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 20:29:27.292
[20:29:27.494][pidpos] Iniciando control PID → SP=5.0 cm
[20:29:28.404][pidpos] Setpoint alcanzado (pos=4.96, SP=5.00, err=0.040)
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=20.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:29:35.597][pidpos] Iniciando control PID → SP=20.0 cm
[20:29:36.692][pidpos] Setpoint alcanzado (pos=19.64, SP=20.00, err=0.358)
[TimeSync] Sincronizado con servidor: 20:29:37.294
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=30.5 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:29:47.438][pidpos] Iniciando control PID → SP=30.5 cm
[TimeSync] Sincronizado con servidor: 20:29:47.296
[20:29:48.397][pidpos] Setpoint alcanzado (pos=30.26, SP=30.50, err=0.244)
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=41.5 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:29:54.789][pidpos] Iniciando control PID → SP=41.5 cm
[20:29:55.908][pidpos] Setpoint alcanzado (pos=41.27, SP=41.50, err=0.233)
[TimeSync] Sincronizado con servidor: 20:29:57.298
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=52.5 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:30:03.482][pidpos] Iniciando control PID → SP=52.5 cm
[20:30:04.595][pidpos] Setpoint alcanzado (pos=52.18, SP=52.50, err=0.321)
[TimeSync] Sincronizado con servidor: 20:30:07.299
[pidpos] Config: Kp=31.80 Ki=2.380 Kd=0.660 SP=60.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[20:30:12.971][pidpos] Iniciando control PID → SP=60.0 cm
[20:30:13.908][pidpos] Setpoint alcanzado (pos=59.72, SP=60.00, err=0.282)
[575435][E][WiFiClient.cpp:429] write(): fail on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 20:30:27.302
[598363][E][WiFiClient.cpp:429] write(): fail on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 20:30:47.304
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 20:31:07.305
[TimeSync] Sincronizado con servidor: 20:31:17.307
```