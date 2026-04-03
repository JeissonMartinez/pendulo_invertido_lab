```zsh
admin@MacBook-Pro-de-Admin SPISC % ~/.platformio/penv/bin/pio device monitor -b 115200
--- forcing DTR inactive
--- forcing RTS inactive
--- Terminal on /dev/cu.Bluetooth-Incoming-Port | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
admin@MacBook-Pro-de-Admin SPISC % ~/.platformio/penv/bin/pio device monitor -b 115200
--- forcing DTR inactive
--- forcing RTS inactive
--- Terminal on /dev/cu.wchusbserial110 | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
[safety] Timeout MQTT — frenando motor
[MqttBridge] MQTT fallo, rc=-2
[MqttBridge] MQTT fallo. Reintento en 1000 ms
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 05:45:28.842
[TimeSync] Sincronizado con servidor: 05:45:38.844
[pidpos] Config: Kp=20.00 Ki=3.000 Kd=0.750 SP=45.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[safety] Timeout MQTT — frenando motor
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 05:45:58.848
[05:46:03.364][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=89.85  pulsos=913  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=22.22 pos_mag=111.90  pulsos=1137  rpm_ir=1500.0 rpm_mag=73.8  useMag=0  err=22.98  pwm=444
[DBG-PID] pos_ir=34.03 pos_mag=123.90  pulsos=1256  rpm_ir=500.0 rpm_mag=1829.3  useMag=0  err=11.07  pwm=300
[DBG-PID] pos_ir=38.89 pos_mag=128.79  pulsos=1305  rpm_ir=0.0 rpm_mag=718.1  useMag=0  err=6.11  pwm=300
[DBG-PID] pos_ir=44.34 pos_mag=134.27  pulsos=1360  rpm_ir=0.0 rpm_mag=368.0  useMag=0  err=0.66  pwm=300
[DBG-PID] pos_ir=45.04 pos_mag=135.01  pulsos=1367  rpm_ir=0.0 rpm_mag=403.6  useMag=0  err=-0.04  pwm=0
[DBG-PID] pos_ir=45.04 pos_mag=135.01  pulsos=1367  rpm_ir=0.0 rpm_mag=9.6  useMag=0  err=-0.04  pwm=0
[05:46:04.685][pidpos] Setpoint alcanzado (pos=45.04, SP=45.00, err=-0.037)
[TimeSync] Sincronizado con servidor: 05:46:08.851
[TimeSync] Sincronizado con servidor: 05:46:18.852
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 05:46:28.854
[pidpos] Homing a INICIO...
[TimeSync] Sincronizado con servidor: 05:46:38.854
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=-57)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[05:46:43.448][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=0.07  pulsos=1831  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=22.12 pos_mag=22.08  pulsos=2054  rpm_ir=1500.0 rpm_mag=38.8  useMag=0  err=23.08  pwm=459
[DBG-PID] pos_ir=34.03 pos_mag=34.11  pulsos=2174  rpm_ir=0.0 rpm_mag=1805.9  useMag=0  err=10.97  pwm=300
[DBG-PID] pos_ir=38.59 pos_mag=38.64  pulsos=2220  rpm_ir=750.0 rpm_mag=768.1  useMag=0  err=6.51  pwm=300
[DBG-PID] pos_ir=44.24 pos_mag=44.29  pulsos=2277  rpm_ir=750.0 rpm_mag=342.4  useMag=0  err=0.86  pwm=300
[DBG-PID] pos_ir=45.24 pos_mag=45.35  pulsos=2287  rpm_ir=0.0 rpm_mag=431.1  useMag=0  err=-0.24  pwm=0
[DBG-PID] pos_ir=45.24 pos_mag=45.35  pulsos=2287  rpm_ir=0.0 rpm_mag=30.2  useMag=0  err=-0.24  pwm=0
[05:46:44.766][pidpos] Setpoint alcanzado (pos=45.24, SP=45.00, err=-0.235)
[TimeSync] Sincronizado con servidor: 05:46:48.854
[TimeSync] Sincronizado con servidor: 05:46:58.855
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-129)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[TimeSync] Sincronizado con servidor: 05:47:08.856
[05:47:13.643][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=90.17 pos_mag=89.78  pulsos=2741  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=67.75 pos_mag=67.53  pulsos=2967  rpm_ir=1500.0 rpm_mag=-1072.3  useMag=0  err=-22.95  pwm=447
[DBG-PID] pos_ir=55.16 pos_mag=54.86  pulsos=3094  rpm_ir=375.0 rpm_mag=-1383.9  useMag=0  err=-10.25  pwm=300
[DBG-PID] pos_ir=48.41 pos_mag=48.06  pulsos=3162  rpm_ir=750.0 rpm_mag=-503.8  useMag=0  err=-3.51  pwm=300
[DBG-PID] pos_ir=44.54 pos_mag=44.08  pulsos=3201  rpm_ir=0.0 rpm_mag=-472.7  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=44.08  pulsos=3201  rpm_ir=0.0 rpm_mag=-0.6  useMag=0  err=0.46  pwm=0
[05:47:14.836][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[TimeSync] Sincronizado con servidor: 05:47:18.858
[TimeSync] Sincronizado con servidor: 05:47:28.859
[TimeSync] Sincronizado con servidor: 05:47:38.861
[TimeSync] Sincronizado con servidor: 05:47:48.863
[588049][E][WiFiClient.cpp:429] write(): fail on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 05:47:58.866
[TimeSync] Sincronizado con servidor: 05:48:08.866
[TimeSync] Sincronizado con servidor: 05:48:18.867
[TimeSync] Sincronizado con servidor: 05:48:28.869
[05:48:34.697][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=44.54 pos_mag=44.09  pulsos=3201  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=44.09  pulsos=3201  rpm_ir=0.0 rpm_mag=0.1  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=44.09  pulsos=3201  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[05:48:35.201][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[TimeSync] Sincronizado con servidor: 05:48:38.871
[05:48:45.081][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=44.54 pos_mag=44.09  pulsos=3201  rpm_ir=0.0 rpm_mag=-0.1  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=44.09  pulsos=3201  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=44.09  pulsos=3201  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[05:48:45.584][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[TimeSync] Sincronizado con servidor: 05:48:48.873
[pidpos] Config: Kp=20.00 Ki=3.000 Kd=0.750 SP=75.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[05:48:54.397][pidpos] Iniciando control PID → SP=75.0 cm
[DBG-PID] pos_ir=44.54 pos_mag=44.09  pulsos=3201  rpm_ir=0.0 rpm_mag=-0.1  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=58.83 pos_mag=58.24  pulsos=3345  rpm_ir=750.0 rpm_mag=344.0  useMag=0  err=16.37  pwm=331
[DBG-PID] pos_ir=65.27 pos_mag=64.78  pulsos=3410  rpm_ir=0.0 rpm_mag=963.7  useMag=0  err=9.73  pwm=300
[DBG-PID] pos_ir=70.04 pos_mag=69.48  pulsos=3458  rpm_ir=1500.0 rpm_mag=383.9  useMag=0  err=4.96  pwm=300
[DBG-PID] pos_ir=75.00 pos_mag=74.55  pulsos=3508  rpm_ir=0.0 rpm_mag=364.2  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=75.09 pos_mag=74.67  pulsos=3509  rpm_ir=0.0 rpm_mag=237.6  useMag=0  err=-0.09  pwm=0
[DBG-PID] pos_ir=75.09 pos_mag=74.67  pulsos=3509  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.09  pwm=0
[05:48:55.678][pidpos] Setpoint alcanzado (pos=75.09, SP=75.00, err=-0.094)
[TimeSync] Sincronizado con servidor: 05:48:58.874
[TimeSync] Sincronizado con servidor: 05:49:08.876
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=-47)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[05:49:17.860][pidpos] Iniciando control PID → SP=75.0 cm
[DBG-PID] pos_ir=-0.10 pos_mag=0.06  pulsos=4269  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=24.80 pos_mag=24.78  pulsos=4521  rpm_ir=2250.0 rpm_mag=732.0  useMag=0  err=50.50  pwm=700
[DBG-PID] pos_ir=56.15 pos_mag=56.13  pulsos=4836  rpm_ir=1500.0 rpm_mag=2420.1  useMag=0  err=19.05  pwm=408
[DBG-PID] pos_ir=67.65 pos_mag=67.88  pulsos=4952  rpm_ir=0.0 rpm_mag=1704.5  useMag=0  err=7.35  pwm=300
[DBG-PID] pos_ir=72.81 pos_mag=72.95  pulsos=5004  rpm_ir=1500.0 rpm_mag=407.4  useMag=0  err=2.29  pwm=300
[DBG-PID] pos_ir=75.39 pos_mag=75.58  pulsos=5030  rpm_ir=0.0 rpm_mag=399.4  useMag=0  err=-0.39  pwm=0
[TimeSync] Sincronizado con servidor: 05:49:18.877
[DBG-PID] pos_ir=75.39 pos_mag=75.58  pulsos=5030  rpm_ir=0.0 rpm_mag=0.1  useMag=0  err=-0.39  pwm=0
[05:49:19.226][pidpos] Setpoint alcanzado (pos=75.39, SP=75.00, err=-0.392)
[TimeSync] Sincronizado con servidor: 05:49:28.879
[TimeSync] Sincronizado con servidor: 05:49:38.880
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-204)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[TimeSync] Sincronizado con servidor: 05:49:48.882
[pidpos] Config: Kp=20.00 Ki=3.000 Kd=0.750 SP=35.5 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[05:49:52.008][pidpos] Iniciando control PID → SP=35.5 cm
[DBG-PID] pos_ir=89.78 pos_mag=89.75  pulsos=5180  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=66.56 pos_mag=66.74  pulsos=5414  rpm_ir=2250.0 rpm_mag=-569.1  useMag=0  err=-31.36  pwm=620
[DBG-PID] pos_ir=46.92 pos_mag=46.88  pulsos=5613  rpm_ir=0.0 rpm_mag=-2138.4  useMag=0  err=-11.42  pwm=300
[DBG-PID] pos_ir=39.58 pos_mag=39.52  pulsos=5686  rpm_ir=0.0 rpm_mag=-808.0  useMag=0  err=-4.08  pwm=300
[DBG-PID] pos_ir=35.22 pos_mag=35.12  pulsos=5730  rpm_ir=0.0 rpm_mag=-527.2  useMag=0  err=0.28  pwm=0
[DBG-PID] pos_ir=35.22 pos_mag=35.12  pulsos=5730  rpm_ir=0.0 rpm_mag=-68.0  useMag=0  err=0.28  pwm=0
[DBG-PID] pos_ir=35.22 pos_mag=35.12  pulsos=5730  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.28  pwm=0
[05:49:53.217][pidpos] Setpoint alcanzado (pos=35.22, SP=35.50, err=0.284)
[TimeSync] Sincronizado con servidor: 05:49:58.882
[TimeSync] Sincronizado con servidor: 05:50:08.884
[734908][E][WiFiClient.cpp:429] write(): fail on fd 48, errno: 104, "Connection reset by peer"
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 05:50:28.887
[safety] Timeout MQTT — frenando motor
[MqttBridge] Intentando MQTT 192.168.1.3:1883...
[MqttBridge] Conectando a MQTT 192.168.1.3:1883 ...
[MqttBridge] MQTT conectado
[MqttBridge] MQTT conectado!
[TimeSync] Sincronizado con servidor: 05:50:48.893
[TimeSync] Sincronizado con servidor: 05:50:58.895
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=-61)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[TimeSync] Sincronizado con servidor: 05:51:08.896
[pidpos] Config: Kp=14.40 Ki=5.240 Kd=1.360 SP=45.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 05:51:18.899
[05:51:23.112][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=0.06  pulsos=6090  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=16.96 pos_mag=16.95  pulsos=6261  rpm_ir=750.0 rpm_mag=171.0  useMag=0  err=28.14  pwm=395
[DBG-PID] pos_ir=28.27 pos_mag=28.31  pulsos=6375  rpm_ir=750.0 rpm_mag=1365.6  useMag=0  err=16.83  pwm=304
[DBG-PID] pos_ir=33.83 pos_mag=33.94  pulsos=6431  rpm_ir=0.0 rpm_mag=707.0  useMag=0  err=11.17  pwm=300
[DBG-PID] pos_ir=39.08 pos_mag=39.20  pulsos=6484  rpm_ir=0.0 rpm_mag=396.7  useMag=0  err=5.92  pwm=300
[DBG-PID] pos_ir=44.64 pos_mag=44.79  pulsos=6540  rpm_ir=0.0 rpm_mag=399.2  useMag=0  err=0.36  pwm=0
[DBG-PID] pos_ir=45.04 pos_mag=45.22  pulsos=6545  rpm_ir=0.0 rpm_mag=357.8  useMag=0  err=-0.04  pwm=0
[DBG-PID] pos_ir=45.04 pos_mag=45.22  pulsos=6545  rpm_ir=0.0 rpm_mag=-0.1  useMag=0  err=-0.04  pwm=0
[05:51:24.617][pidpos] Setpoint alcanzado (pos=45.04, SP=45.00, err=-0.037)
[TimeSync] Sincronizado con servidor: 05:51:28.901
[TimeSync] Sincronizado con servidor: 05:51:38.902
[TimeSync] Sincronizado con servidor: 05:51:48.967
[TimeSync] Sincronizado con servidor: 05:51:58.969
[TimeSync] Sincronizado con servidor: 05:52:08.970
[TimeSync] Sincronizado con servidor: 05:52:18.973
[TimeSync] Sincronizado con servidor: 05:52:28.974
[TimeSync] Sincronizado con servidor: 05:52:38.975
[TimeSync] Sincronizado con servidor: 05:52:48.976
[TimeSync] Sincronizado con servidor: 05:52:58.976
[TimeSync] Sincronizado con servidor: 05:53:08.977
[TimeSync] Sincronizado con servidor: 05:53:18.979
``` 