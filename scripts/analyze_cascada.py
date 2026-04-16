#!/usr/bin/env python3
"""Análisis de datos del test PID Cascada 2026-04-06."""
import csv
import statistics
from collections import Counter

CSV_PATH = "datas/pid_cascada_2026-04-06T06-52-15.csv"

with open(CSV_PATH) as f:
    rows = list(csv.DictReader(f))

print(f"Total muestras: {len(rows)}")
print(f"Rango temporal: {rows[0]['Timestamp']} → {rows[-1]['Timestamp']}")

# ── Distribución de estados ──
states = Counter(r['State'] for r in rows)
print("\n=== DISTRIBUCIÓN DE ESTADOS ===")
for s, c in states.most_common():
    print(f"  {s}: {c} muestras ({100*c/len(rows):.1f}%)")

# ── Segmentos RUNNING ──
running_segments = []
in_running = False
start_idx = 0
for i, r in enumerate(rows):
    if r['State'] == 'PIDC_RUNNING' and not in_running:
        in_running = True
        start_idx = i
    elif r['State'] != 'PIDC_RUNNING' and in_running:
        in_running = False
        running_segments.append((start_idx, i - 1))
if in_running:
    running_segments.append((start_idx, len(rows) - 1))

print(f"\n=== SEGMENTOS RUNNING: {len(running_segments)} ===")
for seg_i, (s, e) in enumerate(running_segments):
    n = e - s + 1
    ts_s = rows[s]['Timestamp']
    ts_e = rows[e]['Timestamp']
    print(f"\n  ── Segmento {seg_i+1}: filas {s+2}–{e+2} ({n} muestras) ──")
    print(f"     Tiempo: {ts_s} → {ts_e}")

    angles = [float(rows[j]['Angle_Pend_deg']) for j in range(s, e + 1)]
    pos = [float(rows[j]['Pos_IR_cm']) for j in range(s, e + 1)]
    pwm_vals = [int(rows[j]['PWM']) for j in range(s, e + 1)]
    ang_err = [float(rows[j]['PIDC_Ang_Error']) for j in range(s, e + 1)]
    ang_out = [float(rows[j]['PIDC_Ang_Output']) for j in range(s, e + 1)]
    ang_out_sat = [float(rows[j]['PIDC_Ang_Output_Sat']) for j in range(s, e + 1)]
    pos_err = [float(rows[j]['PIDC_Pos_Error']) for j in range(s, e + 1)]
    pos_out = [float(rows[j]['PIDC_Pos_Output']) for j in range(s, e + 1)]
    theta_ref = [float(rows[j]['PIDC_Pos_Output_Sat_ThetaRef']) for j in range(s, e + 1)]
    dirs = [rows[j]['Direction'] for j in range(s, e + 1)]
    ls_s_vals = [rows[j]['LS_Start'] for j in range(s, e + 1)]
    ls_e_vals = [rows[j]['LS_End'] for j in range(s, e + 1)]

    sd_ang = statistics.stdev(ang_err) if n > 1 else 0
    sd_pos = statistics.stdev(pos_err) if n > 1 else 0

    print(f"     Ángulo péndulo: min={min(angles):.1f}° max={max(angles):.1f}° mean={statistics.mean(angles):.1f}°")
    print(f"     Error angular:  min={min(ang_err):.2f}° max={max(ang_err):.2f}° mean={statistics.mean(ang_err):.2f}° stdev={sd_ang:.2f}°")
    print(f"     Ang Output (crudo PID): min={min(ang_out):.1f} max={max(ang_out):.1f} mean={statistics.mean(ang_out):.1f}")
    print(f"     Ang Output Sat (PWM±):  min={min(ang_out_sat):.0f} max={max(ang_out_sat):.0f} mean={statistics.mean(ang_out_sat):.0f}")
    print(f"     PWM aplicado al motor:  min={min(pwm_vals)} max={max(pwm_vals)} mean={statistics.mean(pwm_vals):.0f}")
    print(f"     Posición carro: min={min(pos):.1f} cm max={max(pos):.1f} cm mean={statistics.mean(pos):.1f} cm")
    print(f"     Error posición: min={min(pos_err):.2f} max={max(pos_err):.2f} mean={statistics.mean(pos_err):.2f} stdev={sd_pos:.2f}")
    print(f"     Pos Output (crudo PID): min={min(pos_out):.3f} max={max(pos_out):.3f}")
    print(f"     θ_ref (saturado):       min={min(theta_ref):.4f}° max={max(theta_ref):.4f}° mean={statistics.mean(theta_ref):.4f}°")
    print(f"     Dirección motor: {dict(Counter(dirs))}")
    print(f"     LS_Start activo: {sum(1 for x in ls_s_vals if x=='1')} muestras")
    print(f"     LS_End activo:   {sum(1 for x in ls_e_vals if x=='1')} muestras")

    # PWM vs Ang_Output analysis
    print(f"\n     --- Análisis PWM vs salida PID ángulo ---")
    zero_pwm = sum(1 for p in pwm_vals if p == 0)
    high_pwm = sum(1 for p in pwm_vals if p >= 900)
    print(f"     PWM=0 (freno/tolerancia): {zero_pwm} muestras ({100*zero_pwm/n:.1f}%)")
    print(f"     PWM≥900 (saturado):       {high_pwm} muestras ({100*high_pwm/n:.1f}%)")

    # Relationship: |ang_output| + 300 (BIAS) = PWM (when not in deadzone)
    bias_check = []
    for j in range(s, e + 1):
        ao = abs(float(rows[j]['PIDC_Ang_Output']))
        p = int(rows[j]['PWM'])
        if p > 0:
            expected = ao + 300
            bias_check.append((ao, p, expected, p - expected))
    if bias_check:
        diffs = [b[3] for b in bias_check]
        print(f"     PWM - (|output| + BIAS=300): mean={statistics.mean(diffs):.1f} min={min(diffs):.1f} max={max(diffs):.1f}")
        # Show first 5 samples as examples
        print(f"     Ejemplos (|output|, PWM, |output|+300, diff):")
        for ao, p, exp, d in bias_check[:5]:
            print(f"       |output|={ao:.1f} → PWM={p} (esperado={exp:.0f}, diff={d:.0f})")


# ── Análisis de la transición RUNNING→STOPPED ──
print("\n=== TRANSICIONES RUNNING → STOPPED ===")
for i in range(1, len(rows)):
    if rows[i]['State'] == 'PIDC_STOPPED' and rows[i-1]['State'] == 'PIDC_RUNNING':
        print(f"  Fila {i+1}: {rows[i]['Timestamp']}")
        print(f"    Última RUNNING: angle={rows[i-1]['Angle_Pend_deg']}° pos={rows[i-1]['Pos_IR_cm']}cm PWM={rows[i-1]['PWM']} dir={rows[i-1]['Direction']}")
        print(f"    STOPPED:        angle={rows[i]['Angle_Pend_deg']}° pos={rows[i]['Pos_IR_cm']}cm LS_Start={rows[i]['LS_Start']} LS_End={rows[i]['LS_End']}")

# ── Trayectoria del ángulo durante RUNNING más largo ──
if running_segments:
    longest = max(running_segments, key=lambda x: x[1] - x[0])
    s, e = longest
    n = e - s + 1
    print(f"\n=== TRAYECTORIA RUNNING MÁS LARGO ({n} muestras) ===")
    # Show every 10th sample
    print(f"  {'Fila':>5} {'Ángulo':>8} {'AngErr':>8} {'AngOut':>8} {'AngOutSat':>10} {'PWM':>5} {'Dir':>4} {'PosIR':>7} {'PosErr':>8} {'θ_ref':>8}")
    step = max(1, n // 30)
    for j in range(s, e + 1, step):
        r = rows[j]
        print(f"  {j+2:>5} {float(r['Angle_Pend_deg']):>8.1f} {float(r['PIDC_Ang_Error']):>8.2f} {float(r['PIDC_Ang_Output']):>8.1f} {float(r['PIDC_Ang_Output_Sat']):>10.0f} {int(r['PWM']):>5} {r['Direction']:>4} {float(r['Pos_IR_cm']):>7.1f} {float(r['PIDC_Pos_Error']):>8.2f} {float(r['PIDC_Pos_Output_Sat_ThetaRef']):>8.4f}")

# ── Ganancias configuradas ──
print("\n=== GANANCIAS CONFIGURADAS ===")
r = rows[-1]
print(f"  Lazo externo (Posición): Kp={r['PIDC_Pos_Kp']} Ki={r['PIDC_Pos_Ki']} Kd={r['PIDC_Pos_Kd']}")
print(f"  Lazo interno (Ángulo):   Kp={r['PIDC_Ang_Kp']} Ki={r['PIDC_Ang_Ki']} Kd={r['PIDC_Ang_Kd']}")
print(f"  Rango trabajo={r['PIDC_Rango_Trabajo']}° Tol_ang={r['PIDC_Tolerancia_Ang']}° PWM_min={r['PIDC_PWM_Min']} PWM_max={r['PIDC_PWM_Max']} θ_ref_max={r['PIDC_Theta_Ref_Max']}°")
print(f"  Pos setpoint={r['PIDC_Pos_Setpoint']}cm Zona seg={r['PIDC_Pos_Zona_Seg']}cm")
