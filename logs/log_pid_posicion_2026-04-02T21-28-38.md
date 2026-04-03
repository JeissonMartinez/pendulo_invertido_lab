```zsh
admin@MacBook-Pro-de-Admin SPISC % ~/.platformio/penv/bin/pio device monitor -b 115200 
--- forcing DTR inactive
--- forcing RTS inactive
--- Terminal on /dev/cu.wchusbserial110 | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
[TimeSync] Sincronizado con servidor: 21:26:47.934
[TimeSync] Sincronizado con servidor: 21:26:57.936
[main] Cambio de modo → PID_POS
[pidpos] Modo PID Posición activado.
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=-61090)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[TimeSync] Sincronizado con servidor: 21:27:07.937
[TimeSync] Sincronizado con servidor: 21:27:17.938
[pidpos] Config: Kp=17.50 Ki=3.730 Kd=0.770 SP=45.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[TimeSync] Sincronizado con servidor: 21:27:27.940
[21:27:29.700][pidpos] Iniciando control PID → SP=45.0 cm
[21:27:31.298][pidpos] Setpoint alcanzado (pos=44.84, SP=45.00, err=0.162)
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-65340)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[TimeSync] Sincronizado con servidor: 21:27:37.942
[21:27:41.232][pidpos] Iniciando control PID → SP=45.0 cm
[21:27:42.676][pidpos] Setpoint alcanzado (pos=44.84, SP=45.00, err=0.162)
[pidpos] Homing a INICIO...
[TimeSync] Sincronizado con servidor: 21:27:47.944
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=-61167)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[21:27:53.228][pidpos] Iniciando control PID → SP=45.0 cm
[21:27:54.820][pidpos] Setpoint alcanzado (pos=44.84, SP=45.00, err=0.162)
[TimeSync] Sincronizado con servidor: 21:27:57.946
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-61250)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[21:28:04.961][pidpos] Iniciando control PID → SP=45.0 cm
[21:28:06.394][pidpos] Setpoint alcanzado (pos=44.94, SP=45.00, err=0.062)
[TimeSync] Sincronizado con servidor: 21:28:07.948
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=-56980)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[21:28:16.154][pidpos] Iniciando control PID → SP=45.0 cm
[21:28:17.704][pidpos] Setpoint alcanzado (pos=44.74, SP=45.00, err=0.261)
[TimeSync] Sincronizado con servidor: 21:28:17.950
[TimeSync] Sincronizado con servidor: 21:28:27.952
```