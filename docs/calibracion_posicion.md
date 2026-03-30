# Calibración del Sensor de Posición AS5600 (Carro)

**Proyecto:** [SPISC — Sistema de Péndulo Invertido con Supervisión y Control](../README.md)  
**Fecha:** 30/03/2026  
**Autor:** Ing. Jeisson Martínez Flórez

---

## Tabla de contenidos

- [Descripción general](#descripción-general)
- [Conexión del hardware](#conexión-del-hardware)
- [Configuración programada](#configuración-programada)
- [Funciones implementadas](#funciones-implementadas)
- [Problemas encontrados](#problemas-encontrados)
- [Soluciones implementadas](#soluciones-implementadas)
- [Pruebas de repetibilidad](#pruebas-de-repetibilidad)
- [Instructivo de calibración](#instructivo-de-calibración)

---

## Descripción general

El sensor **AS5600** es un encoder magnético absoluto de 12 bits que se utiliza para medir la posición lineal del carro sobre el riel. El imán está acoplado al eje del motor/polea, y al girar, el sensor detecta el ángulo absoluto del imán mediante efecto Hall.

La posición acumulada se calcula contando las revoluciones completas del eje y convirtiendo a centímetros mediante una constante de calibración.

---

## Conexión del hardware

### Bus I2C asignado

| Parámetro | Valor |
|-----------|-------|
| **Bus I2C** | Wire1 (Bus 1) |
| **Pin SDA** | GPIO 18 |
| **Pin SCL** | GPIO 19 |
| **Dirección I2C** | 0x36 |
| **Velocidad** | 400 kHz (Fast Mode) |

### Dispositivos en el mismo bus

El Bus 1 (Wire1) es compartido con:
- Pantalla OLED SSD1306 (dirección 0x3C)

### Montaje físico del imán

- **Tipo de imán:** Neodimio diametralmente magnetizado
- **Distancia imán-sensor:** 1-3 mm (óptimo ~2 mm)
- **Acoplamiento:** Concéntrico al eje del motor/polea
- **Orientación:** Cara del imán paralela al chip AS5600

---

## Configuración programada

### Constantes de calibración

Archivo: [`SPISC/include/hardware_pins.h`](../SPISC/include/hardware_pins.h)

```cpp
#define I2C_BUS1_SDA     18
#define I2C_BUS1_SCL     19
#define AS5600_ADDR      0x36
#define CM_POR_GRADO     0.01103f  // Calibrado: 90cm / 8158° = 0.01103 cm/°
```

### Variables de estado

Archivo: [`SPISC/src/sensors_i2c.cpp`](../SPISC/src/sensors_i2c.cpp)

```cpp
static AS5600 _as5600_cart(&Wire1);     // Instancia del sensor
static float  _posMagCm    = 0;         // Posición en centímetros
static float  _rpsMag      = 0;         // Revoluciones por segundo
static float  _rpmMag      = 0;         // Revoluciones por minuto
static int32_t _posOffset  = 265;       // Offset calibrado (cumPos cuando LS INICIO activo)
static bool   _calibratedMagPos = false;
```

### Fórmula de conversión

```cpp
// cumPos: posición acumulada del encoder (pasos)
// 4096 pasos = 1 revolución = 360°
// CM_POR_GRADO = 0.01103 cm/°
_posMagCm = ((cumPos - _posOffset) / 4096.0f) * 360.0f * CM_POR_GRADO;
```

---

## Funciones implementadas

### Namespace: `SensorsI2C`

| Función | Descripción |
|---------|-------------|
| `begin()` | Inicializa buses I2C y resetea posición acumulada |
| `update()` | Lee sensores y actualiza variables (llamar en loop) |
| `getPosMagCm()` | Retorna posición del carro en centímetros |
| `getRpsMag()` | Retorna velocidad en revoluciones/segundo |
| `getRpmMag()` | Retorna velocidad en revoluciones/minuto |
| `resetMagPosition()` | Resetea posición acumulada a 0 |
| `isMagPosCalibrated()` | Indica si la posición fue calibrada |

### Ejemplo de uso

```cpp
#include "sensors_i2c.h"

void setup() {
    SensorsI2C::begin();
}

void loop() {
    SensorsI2C::update();
    
    float posicion = SensorsI2C::getPosMagCm();
    float rpm = SensorsI2C::getRpmMag();
    
    Serial.printf("Posición: %.2f cm, RPM: %.1f\n", posicion, rpm);
}
```

---

## Problemas encontrados

### Problema 1: Imán desacoplado del eje

**Síntoma:** El valor de `cumPos` apenas variaba (~114 pasos) al mover el carro 90 cm completos.

**Diagnóstico:**
```
[debug AS5600 carro] cumPos=-64, posMagCm=-0.07, i2cErr=0
[debug AS5600 carro] cumPos=-65, posMagCm=-0.07, i2cErr=0
...
[debug AS5600 carro] cumPos=33, posMagCm=0.03, i2cErr=0
```

**Causa:** El imán estaba colocado lateralmente sobre el eje, no concéntricamente. Posiblemente se desajustó por un golpe o movimiento brusco del prototipo.

### Problema 2: Campo magnético débil / imán no detectado

**Síntoma:** `detected=NO`, `mag` bajo (~70), `agc=128`

**Diagnóstico:**
```
[debug AS5600 carro] rawAngle=750, cumPos=-1, mag=72, agc=128, detected=NO, i2cErr=0
```

**Causa:** Imán mal alineado o demasiado lejos del sensor.

### Problema 3: Constante CM_POR_GRADO incorrecta

**Síntoma inicial:** Con datos erróneos del imán desacoplado, se calculó `CM_POR_GRADO = 1.66` (incorrecto).

**Causa:** Los primeros datos de calibración (617 pasos para 90 cm) estaban basados en lecturas con el imán mal montado.

---

## Soluciones implementadas

### Solución 1: Corrección del montaje del imán

1. Verificar que el imán esté centrado concéntricamente al eje
2. Ajustar distancia a 2-3 mm del chip AS5600
3. Asegurar fijación mecánica del imán

**Resultado después de corrección:**
```
[debug AS5600 carro] rawAngle=3080, cumPos=0, mag=2068, agc=51, detected=SI, i2cErr=0
```
- `detected=SI` ✓
- `mag ≈ 2100` (fuerte) ✓
- `agc ≈ 50` (ganancia baja = señal fuerte) ✓

### Solución 2: Calibración de CM_POR_GRADO

Con el imán correctamente montado, los datos reales fueron:
- Recorrido: 90 cm
- Pasos acumulados: 92,814

**Cálculo:**
```
Revoluciones = 92814 / 4096 = 22.66
Grados totales = 22.66 × 360° = 8158°
CM_POR_GRADO = 90 cm / 8158° = 0.01103 cm/°
```

### Solución 3: Offset de posición inicial

Cuando el limit switch INICIO está activo, `cumPos ≈ 265` (no exactamente 0).

**Implementación:**
```cpp
static int32_t _posOffset = 265;  // Calibrado
_posMagCm = ((cumPos - _posOffset) / 4096.0f) * 360.0f * CM_POR_GRADO;
```

---

## Pruebas de repetibilidad

### Metodología

Se realizaron 5 viajes completos de ida y vuelta (inicio → fin → inicio), registrando:
- Posición cuando se activa cada limit switch
- Valor de `cumPos` en cada evento
- Parámetros del sensor (`rawAngle`, `mag`, `agc`)

### Resultados

#### Eventos en Limit Switch FIN

| Viaje | cumPos | posMagCm |
|-------|--------|----------|
| 1 | 92787 | 89.95 cm |
| 2 | 92788 | 89.95 cm |
| 3 | 92782 | 89.95 cm |
| 4 | 92786 | 89.95 cm |
| 5 | 92786 | 89.95 cm |
| **Promedio** | **92785.8** | **89.95 cm** |

#### Eventos en Limit Switch INICIO

| Viaje | cumPos | posMagCm |
|-------|--------|----------|
| 1 | 269 | 0.26 cm |
| 2 | 268 | 0.26 cm |
| 3 | 258 | 0.25 cm |
| 4 | 265 | 0.26 cm |
| 5 | 267 | 0.26 cm |
| **Promedio** | **265.4** | **0.26 cm** |

### Métricas de calidad

| Métrica | Valor |
|---------|-------|
| Repetibilidad en FIN | ±6 pasos (0.006%) |
| Repetibilidad en INICIO | ±11 pasos |
| Recorrido útil medido | 89.69 cm |
| Error vs 90 cm esperados | 0.34% |
| Campo magnético (mag) | 2049-2157 (estable) |
| Estado de detección | `detected=SI` constante |

### Datos crudos de muestra

```
[debug] rawAngle=3081, cumPos=0, mag=2110, posMagCm=0.00 cm, LS:[INI|---]
[LS] INICIO inactivo | cumPos=334, posMagCm=0.32
[debug] rawAngle=953, cumPos=1964, mag=2138, posMagCm=1.90 cm, LS:[---|---]
...
[debug] rawAngle=1099, cumPos=92220, mag=2108, posMagCm=89.40 cm, LS:[---|---]
[LS] FIN ACTIVO | cumPos=92787, posMagCm=89.95
[debug] rawAngle=1694, cumPos=92821, mag=2093, posMagCm=89.98 cm, LS:[---|FIN]
```

---

## Instructivo de calibración

### Cuándo recalibrar

- Después de modificar el montaje mecánico del imán
- Si se observa deriva en las lecturas de posición
- Si cambia la relación de transmisión (polea/engranaje)
- Después de un impacto o golpe fuerte en el prototipo

### Herramientas necesarias

1. Acceso al Serial Monitor (115200 baud)
2. Flexómetro o regla para medir recorrido real
3. Herramientas para ajustar el montaje del imán

### Procedimiento paso a paso

#### Paso 1: Agregar código de debug

En `SPISC/src/sensors_i2c.cpp`, después de calcular `_posMagCm`, agregar:

```cpp
// DEBUG temporal para calibración
static unsigned long lastDebugMs = 0;
if (millis() - lastDebugMs >= 500) {
    int rawAngle = _as5600_cart.readAngle();
    int mag = _as5600_cart.readMagnitude();
    bool detected = _as5600_cart.detectMagnet();
    Serial.printf("[debug] rawAngle=%d, cumPos=%ld, mag=%d, detected=%s\n", 
                  rawAngle, cumPos, mag, detected ? "SI" : "NO");
    lastDebugMs = millis();
}
```

#### Paso 2: Verificar detección del imán

Flashear y observar Serial Monitor:
- `detected=SI` → Imán detectado ✓
- `detected=NO` → Revisar montaje/distancia del imán
- `mag > 1500` → Campo magnético adecuado
- `mag < 500` → Imán muy lejos o mal orientado

#### Paso 3: Verificar movimiento del rawAngle

Mover el carro manualmente y observar:
- `rawAngle` debe variar continuamente (0-4095)
- Si `rawAngle` no cambia → el imán no gira con el eje

#### Paso 4: Medir recorrido y calcular CM_POR_GRADO

1. Colocar carro en posición INICIO (LS activo)
2. Anotar `cumPos` inicial
3. Mover carro hasta posición FIN (LS activo)
4. Anotar `cumPos` final
5. Medir recorrido real en centímetros

**Fórmula:**
```
delta_cumPos = cumPos_final - cumPos_inicial
grados = (delta_cumPos / 4096) × 360
CM_POR_GRADO = recorrido_cm / grados
```

#### Paso 5: Calcular offset de posición

Con el carro en posición INICIO (LS activo):
```
_posOffset = cumPos actual (típicamente ~265)
```

#### Paso 6: Actualizar constantes

Editar `SPISC/include/hardware_pins.h`:
```cpp
#define CM_POR_GRADO     0.01103f  // Actualizar con valor calculado
```

Editar `SPISC/src/sensors_i2c.cpp`:
```cpp
static int32_t _posOffset = 265;  // Actualizar con valor medido
```

#### Paso 7: Validar calibración

1. Compilar y flashear
2. Verificar que `posMagCm ≈ 0.00` en posición INICIO
3. Verificar que `posMagCm ≈ 90.00` en posición FIN
4. Realizar múltiples viajes para confirmar repetibilidad

#### Paso 8: Eliminar código de debug

Remover el bloque de debug agregado en el Paso 1.

---

## Referencias

- [Datasheet AS5600](https://ams.com/documents/20143/36005/AS5600_DS000365_5-00.pdf)
- [README principal del proyecto](../README.md)
- [Plan de mejoras](plan_mejoras.md)

---

<p align="center">
  <sub>Documentación generada para el proyecto SPISC — 30/03/2026</sub>
</p>
