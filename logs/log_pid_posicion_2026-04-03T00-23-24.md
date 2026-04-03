```zsh
admin@MacBook-Pro-de-Admin SPISC % ~/.platformio/penv/bin/pio device monitor -b 115200
--- forcing DTR inactive
--- forcing RTS inactive
--- Terminal on /dev/cu.wchusbserial110 | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
[TimeSync] Sincronizado con servidor: 00:20:46.800
[TimeSync] Sincronizado con servidor: 00:20:56.801
[main] Cambio de modo → PID_POS
[pidpos] Modo PID Posición activado.
[pidpos] Config: Kp=22.80 Ki=2.670 Kd=0.500 SP=45.0 ZS=5.0 TOL=0.5
[pidpos] Config: Sensor=IR PWM=[300,700]
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=0)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[00:21:06.395][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=0.00  pulsos=318  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=22.62 pos_mag=0.00  pulsos=546  rpm_ir=3000.0 rpm_mag=0.0  useMag=0  err=22.58  pwm=517
[DBG-PID] pos_ir=37.00 pos_mag=0.00  pulsos=691  rpm_ir=600.0 rpm_mag=0.0  useMag=0  err=8.20  pwm=300
[TimeSync] Sincronizado con servidor: 00:21:06.803
[DBG-PID] pos_ir=42.26 pos_mag=0.00  pulsos=744  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=2.74  pwm=300
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=773  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=773  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[00:21:07.572][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[TimeSync] Sincronizado con servidor: 00:21:16.804
[TimeSync] Sincronizado con servidor: 00:21:26.806
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-92837)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[TimeSync] Sincronizado con servidor: 00:21:36.807
[00:21:39.882][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=89.97 pos_mag=90.00  pulsos=1226  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=66.17 pos_mag=90.00  pulsos=1466  rpm_ir=1500.0 rpm_mag=0.0  useMag=0  err=-21.27  pwm=479
[DBG-PID] pos_ir=51.98 pos_mag=90.00  pulsos=1609  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-6.98  pwm=300
[DBG-PID] pos_ir=46.03 pos_mag=90.00  pulsos=1669  rpm_ir=1500.0 rpm_mag=0.0  useMag=0  err=-1.13  pwm=300
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=1681  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=1681  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[00:21:41.023][pidpos] Setpoint alcanzado (pos=45.43, SP=45.00, err=-0.434)
[TimeSync] Sincronizado con servidor: 00:21:46.809
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=0)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[00:21:54.588][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=0.00  pulsos=2138  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=23.71 pos_mag=0.00  pulsos=2377  rpm_ir=2250.0 rpm_mag=0.0  useMag=0  err=21.59  pwm=489
[DBG-PID] pos_ir=37.50 pos_mag=0.00  pulsos=2516  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=7.50  pwm=300
[DBG-PID] pos_ir=44.44 pos_mag=0.00  pulsos=2586  rpm_ir=750.0 rpm_mag=0.0  useMag=0  err=0.76  pwm=300
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=2594  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=2594  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[00:21:55.702][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[TimeSync] Sincronizado con servidor: 00:21:56.809
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-92837)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[00:22:01.305][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=89.97 pos_mag=90.00  pulsos=3048  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=68.25 pos_mag=90.00  pulsos=3267  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-23.25  pwm=491
[DBG-PID] pos_ir=52.28 pos_mag=90.00  pulsos=3428  rpm_ir=750.0 rpm_mag=0.0  useMag=0  err=-7.48  pwm=300
[DBG-PID] pos_ir=45.33 pos_mag=90.00  pulsos=3500  rpm_ir=1500.0 rpm_mag=0.0  useMag=0  err=-0.33  pwm=0
[DBG-PID] pos_ir=45.33 pos_mag=90.00  pulsos=3506  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.33  pwm=0
[DBG-PID] pos_ir=45.33 pos_mag=90.00  pulsos=3506  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.33  pwm=0
[00:22:02.402][pidpos] Setpoint alcanzado (pos=45.33, SP=45.00, err=-0.334)
[pidpos] Homing a FIN...
[TimeSync] Sincronizado con servidor: 00:22:06.811
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-92837)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[00:22:10.914][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=89.97 pos_mag=90.00  pulsos=3969  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=68.05 pos_mag=90.00  pulsos=4190  rpm_ir=1500.0 rpm_mag=0.0  useMag=0  err=-23.15  pwm=510
[DBG-PID] pos_ir=52.58 pos_mag=90.00  pulsos=4346  rpm_ir=750.0 rpm_mag=0.0  useMag=0  err=-7.97  pwm=300
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=4428  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=4429  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=4430  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[00:22:11.977][pidpos] Setpoint alcanzado (pos=45.43, SP=45.00, err=-0.434)
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=0)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=0)
[main] Auto-calibración: LS_START → posición = 0 cm
[TimeSync] Sincronizado con servidor: 00:22:16.812
[00:22:18.995][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=0.00  pulsos=4879  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=22.22 pos_mag=0.00  pulsos=5103  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=22.78  pwm=513
[DBG-PID] pos_ir=37.70 pos_mag=0.00  pulsos=5259  rpm_ir=375.0 rpm_mag=0.0  useMag=0  err=7.40  pwm=300
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=5331  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=5335  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=5335  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[00:22:20.086][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-92837)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[TimeSync] Sincronizado con servidor: 00:22:26.813
[00:22:28.530][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=89.97 pos_mag=90.00  pulsos=5791  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=66.56 pos_mag=90.00  pulsos=6027  rpm_ir=3000.0 rpm_mag=0.0  useMag=0  err=-22.16  pwm=509
[DBG-PID] pos_ir=50.49 pos_mag=90.00  pulsos=6189  rpm_ir=750.0 rpm_mag=0.0  useMag=0  err=-5.59  pwm=300
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=6248  rpm_ir=750.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=6248  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=6248  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[00:22:29.580][pidpos] Setpoint alcanzado (pos=45.43, SP=45.00, err=-0.434)
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-92837)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[00:22:36.769][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=89.97 pos_mag=90.00  pulsos=6711  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[TimeSync] Sincronizado con servidor: 00:22:36.814
[DBG-PID] pos_ir=66.96 pos_mag=90.00  pulsos=6943  rpm_ir=1500.0 rpm_mag=0.0  useMag=0  err=-22.26  pwm=491
[DBG-PID] pos_ir=52.18 pos_mag=90.00  pulsos=7092  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-7.18  pwm=300
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=7166  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=7171  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=7171  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[00:22:37.844][pidpos] Setpoint alcanzado (pos=45.43, SP=45.00, err=-0.434)
[pidpos] Homing a INICIO...
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=0)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[00:22:46.136][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=0.00  pulsos=7636  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=23.31 pos_mag=0.00  pulsos=7871  rpm_ir=2250.0 rpm_mag=0.0  useMag=0  err=22.28  pwm=497
[DBG-PID] pos_ir=36.90 pos_mag=0.00  pulsos=8008  rpm_ir=750.0 rpm_mag=0.0  useMag=0  err=8.20  pwm=300
[DBG-PID] pos_ir=41.96 pos_mag=0.00  pulsos=8059  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=3.04  pwm=300
[TimeSync] Sincronizado con servidor: 00:22:46.815
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=8089  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=8089  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=8089  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[00:22:47.359][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-92837)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[00:22:54.313][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=89.97 pos_mag=90.00  pulsos=8547  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=66.66 pos_mag=90.00  pulsos=8782  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-21.66  pwm=458
[DBG-PID] pos_ir=50.99 pos_mag=90.00  pulsos=8940  rpm_ir=750.0 rpm_mag=0.0  useMag=0  err=-6.09  pwm=300
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=9003  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=9003  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=9003  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[00:22:55.371][pidpos] Setpoint alcanzado (pos=45.43, SP=45.00, err=-0.434)
[TimeSync] Sincronizado con servidor: 00:22:56.816
[pidpos] Homing a FIN...
[EncoderIR] Posición calibrada a 90.0 cm (907 pulsos)
[sensors] Posición magnética calibrada a 90.0 cm (offset=-92837)
[main] Auto-calibración: LS_END → posición = 90 cm
[pidpos] Home (fin) alcanzado.
[00:23:02.739][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=89.97 pos_mag=90.00  pulsos=9467  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=66.46 pos_mag=90.00  pulsos=9704  rpm_ir=3000.0 rpm_mag=0.0  useMag=0  err=-21.66  pwm=477
[DBG-PID] pos_ir=51.98 pos_mag=90.00  pulsos=9850  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-6.98  pwm=300
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=9918  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=9924  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[DBG-PID] pos_ir=45.43 pos_mag=90.00  pulsos=9924  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=-0.43  pwm=0
[00:23:03.838][pidpos] Setpoint alcanzado (pos=45.43, SP=45.00, err=-0.434)
[pidpos] Homing a INICIO...
[TimeSync] Sincronizado con servidor: 00:23:06.818
[EncoderIR] Posición calibrada a 0.0 cm (0 pulsos)
[sensors] Posición magnética calibrada a 0.0 cm (offset=0)
[main] Auto-calibración: LS_START → posición = 0 cm
[pidpos] Home (inicio) alcanzado.
[00:23:09.481][pidpos] Iniciando control PID → SP=45.0 cm
[DBG-PID] pos_ir=0.00 pos_mag=0.00  pulsos=10378  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.00  pwm=0
[DBG-PID] pos_ir=23.21 pos_mag=0.00  pulsos=10612  rpm_ir=2000.0 rpm_mag=0.0  useMag=0  err=22.18  pwm=501
[DBG-PID] pos_ir=37.10 pos_mag=0.00  pulsos=10752  rpm_ir=500.0 rpm_mag=0.0  useMag=0  err=8.00  pwm=300
[DBG-PID] pos_ir=42.26 pos_mag=0.00  pulsos=10804  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=2.74  pwm=300
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=10831  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[DBG-PID] pos_ir=44.54 pos_mag=0.00  pulsos=10831  rpm_ir=0.0 rpm_mag=0.0  useMag=0  err=0.46  pwm=0
[00:23:10.676][pidpos] Setpoint alcanzado (pos=44.54, SP=45.00, err=0.459)
[TimeSync] Sincronizado con servidor: 00:23:16.819
```