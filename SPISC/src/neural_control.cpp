#include <Arduino.h>
#include <Neural_Networks_FF.h> // Asumiendo que los espacios eran guiones bajos
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AS5600.h>

// --- Configuración de hardware ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define SDA_PIN1 21
#define SCL_PIN1 22
#define SDA_PIN2 18
#define SCL_PIN2 19

#define PWM_FREQ 25000
#define PWM_RES 10

// Pines del motor
const int R_EN_PIN = 26;
const int L_EN_PIN = 27;
const int R_PWM_PIN = 14;
const int L_PWM_PIN = 12;

// --- Variables del sistema ---
volatile float posicionCm = 0.0;
volatile float posicionSetpoint = 45.0; // Posición inicial central
int motorSpeed = 0;
bool motorEnabled = true;
String motorDirection = "DETENIDO";

// --- Constantes y variables de control ---
long vueltas = 0;
const float ANGLE_SETPOINT = 180.0; // Objetivo: vertical
float MIN_ANGLE = ANGLE_SETPOINT - 50.0; // 130.0
float MAX_ANGLE = ANGLE_SETPOINT + 50.0; // 230.0

float MIN_POSITION = 20.0;
float MAX_POSITION = 80.0;
float MAX_POS_ADJUST = 2.0;
float ZONA_ALERTA = 10.0;
float KP_POS = 0.5; // Ganancia del lazo de control de posición

// --- Calibración y estado del sistema ---
float angleOffset = 0.0;
bool sistemaIniciado = false;
bool calibradoAngulo = false;
bool calibradoPosicion = false;
bool menuMostrado = false;

volatile int rev = 0;
unsigned long lastMicros = 0;
volatile long posicionAcumulada = 0;
const float CM_POR_GRADO = 0.0115780;
const unsigned long SAMPLE_PERIOD = 2000; // 2 ms

int32_t lastRawPos = 0;
float Posc = 0.00;
bool initialPositionReached = false;
float anguloSP = 0.00;

float error = 0.00;
float error_1 = 0.00;
float error_2 = 0.00;
float Output = 0.00;
float front = 0.0;
float back = 0.0;

// Variables de sensores
float anguloSensor1 = 0.0;
float anguloSensor2 = 0.0;
float anguloAnterior2 = 0.0;

// --- Objetos ---
TwoWire I2Cuno = TwoWire(0);
TwoWire I2Cdos = TwoWire(1);

AS5600 as5600_1(&I2Cuno);
AS5600 as5600_2(&I2Cdos);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cuno);

Neural_Networks_FF net = Neural_Networks_FF();
Dynamic_Array EST = Dynamic_Array();
Dynamic_Array FAC_STR = Dynamic_Array();
Dynamic_Array XI = Dynamic_Array();
Dynamic_Array D = Dynamic_Array();
Dynamic_Array posicion = Dynamic_Array();


// ================= FUNCIONES DEL MOTOR =================

void motorAdelante() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(1, 0);          // L apagado
  ledcWrite(0, motorSpeed); // R encendido
}

void motorAtras() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(0, 0);          // R apagado
  ledcWrite(1, motorSpeed); // L encendido
}

void motorFreno() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(0, 0);
  ledcWrite(1, 0);
}

// ================= LECTURA DE SENSORES =================

void leerSensoresCada2ms() {
  unsigned long now = micros();
  if ((now - lastMicros) >= SAMPLE_PERIOD) {
    lastMicros = now;
    
    // --- Sensor 1 (ángulo) con offset de calibración ---
    float rawAngle = map(as5600_1.readAngle(), 0, 4095, 3600, 0) / 10.0;
    anguloSensor1 = fmod(rawAngle - angleOffset + 360.0, 360.0);
    
    // --- Sensor 2 (posición) ---
    int32_t rawPos = as5600_2.getCumulativePosition();
    int32_t delta = rawPos - lastRawPos;
    
    // Detección de vuelta
    if (delta < -2000) vueltas++;
    if (delta > 2000) vueltas--;
    
    lastRawPos = rawPos;
    posicionAcumulada = (vueltas * 360) + map(rawPos, 0, 4095, 0, 360);
    posicionCm = posicionAcumulada * CM_POR_GRADO;
  }
}

// ================= RED NEURONAL =================

float angleControl(float currentAngle, float setpoint) {
  error = setpoint - currentAngle;
  
  posicion.WriteArray_2D(0, 0, currentAngle / 1000.00);
  XI.WriteArray_2D(0, 0, error);
  XI.WriteArray_2D(0, 1, error - error_1);
  XI.WriteArray_2D(0, 2, error_1 - error_2);
  
  net.NORMALIZE_DATA(XI, 1000.00);
  D.WriteArray_2D(0, 0, setpoint / 1000.00);
  
  net.TRAIN_NET_ONLINE(XI, D, posicion);
  Output = net.y.ReadArray_2D(net.y.GetRows() - 1, 0);
  
  return Output * 1000.00;
}

// ================= INTERFAZ Y SERIAL =================

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Angulo: ");
  
  display.setCursor(0, 10);
  display.setTextSize(2);
  display.print(anguloSensor1, 1); 

  display.setTextSize(1);
  display.setCursor(70, 0);
  if (!motorEnabled) {
    display.print("ALERTA");
  } else if (!initialPositionReached) {
    display.print("INICIAL");
  } else {
    display.print("VERTICAL");
  }

  display.setCursor(0, 30);
  display.print("Posicion: ");
  display.setCursor(0, 40);
  display.setTextSize(2);
  display.print(posicionCm, 1);
  display.print("cm");

  display.setTextSize(1);
  display.setCursor(70, 15);
  display.print("PWM: ");
  display.print(motorSpeed);

  if ((posicionCm < (MIN_POSITION + ZONA_ALERTA)) || (posicionCm > (MAX_POSITION - ZONA_ALERTA))) {
    display.setCursor(70, 25);
    display.print("!ZONA CRIT!");
  }

  display.setCursor(70, 40);
  display.print("Dir: ");
  display.setCursor(70, 50);
  display.print(motorDirection);

  display.display();
}

void printSystemStatus() {
  Serial.print("Angulo: "); Serial.print(anguloSensor1, 1);
  Serial.print(" | Posicion: "); Serial.print(posicionCm, 1); Serial.print(" cm");
  Serial.print(" | PWM: "); Serial.print(motorSpeed);
  Serial.print(" | Dir: "); Serial.println(motorDirection);
}

// ================= CALIBRACION =================

void calibrarAngulo() {
  Serial.println();
  Serial.println("--- CALIBRACION DE ANGULO ---");
  Serial.println("Asegure que el pendulo este vertical hacia ABAJO.");
  Serial.println("Presione Enter para confirmar...");
  
  while (!Serial.available()) {
    leerSensoresCada2ms();
    updateDisplay();
    delay(10);
  }
  while (Serial.available()) { Serial.read(); } // Limpiar buffer
  
  // Leer ángulo crudo actual (sin offset)
  angleOffset = map(as5600_1.readAngle(), 0, 4095, 3600, 0) / 10.0;
  calibradoAngulo = true;
  
  Serial.print("Angulo calibrado. Offset almacenado: ");
  Serial.print(angleOffset, 1);
  Serial.println(" grados");
  Serial.println("Posicion abajo = 0 grados, arriba = 180 grados.");
  Serial.println();
}

void calibrarPosicion() {
  Serial.println();
  Serial.println("--- CALIBRACION DE POSICION ---");
  Serial.println("Asegure que el carro este en la posicion inicial (0 cm).");
  Serial.println("Presione Enter para confirmar...");
  
  while (!Serial.available()) {
    leerSensoresCada2ms();
    updateDisplay();
    delay(10);
  }
  while (Serial.available()) { Serial.read(); } // Limpiar buffer
  
  // Resetear todo lo relacionado con posición
  as5600_2.resetPosition();
  as5600_2.resetCumulativePosition();
  vueltas = 0;
  lastRawPos = 0;
  posicionAcumulada = 0;
  posicionCm = 0.0;
  calibradoPosicion = true;
  
  Serial.println("Posicion calibrada. Posicion actual: 0.0 cm");
  Serial.println();
}

// ================= MENU SERIAL =================

void mostrarMenu() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("         PENDULO INVERTIDO");
  Serial.println("========================================");
  Serial.println("Que desea hacer?");
  Serial.print("  1. Calibrar angulo");
  if (!calibradoAngulo) Serial.print("       (!) No calibrado");
  Serial.println();
  Serial.print("  2. Calibrar posicion");
  if (!calibradoPosicion) Serial.print("     (!) No calibrado");
  Serial.println();
  Serial.print("  3. Iniciar rutina");
  if (!calibradoAngulo || !calibradoPosicion) Serial.print("        (!) Calibre primero");
  Serial.println();
  Serial.println("  4. Configurar parametros");
  Serial.println("========================================");
  Serial.println("Ingrese opcion (1-4):");
  menuMostrado = true;
}

void configurarParametros() {
  while (true) {
    Serial.println();
    Serial.println("--- PARAMETROS DEL SISTEMA ---");
    Serial.print("  a. MIN_ANGLE    = "); Serial.println(MIN_ANGLE, 1);
    Serial.print("  b. MAX_ANGLE    = "); Serial.println(MAX_ANGLE, 1);
    Serial.print("  c. MIN_POSITION = "); Serial.println(MIN_POSITION, 1);
    Serial.print("  d. MAX_POSITION = "); Serial.println(MAX_POSITION, 1);
    Serial.print("  e. ZONA_ALERTA  = "); Serial.println(ZONA_ALERTA, 1);
    Serial.print("  f. KP_POS       = "); Serial.println(KP_POS, 2);
    Serial.println("Ingrese letra del parametro a cambiar (o 'x' para volver):");
    
    while (!Serial.available()) {
      leerSensoresCada2ms();
      updateDisplay();
      delay(10);
    }
    char opcion = Serial.read();
    while (Serial.available()) { Serial.read(); } // Limpiar buffer
    
    if (opcion == 'x' || opcion == 'X') break;
    
    if (opcion >= 'a' && opcion <= 'f') {
      Serial.print("Ingrese nuevo valor: ");
      while (!Serial.available()) {
        leerSensoresCada2ms();
        updateDisplay();
        delay(10);
      }
      String valorStr = Serial.readStringUntil('\n');
      valorStr.trim();
      float nuevoValor = valorStr.toFloat();
      
      switch (opcion) {
        case 'a': MIN_ANGLE = nuevoValor; Serial.print("MIN_ANGLE = "); break;
        case 'b': MAX_ANGLE = nuevoValor; Serial.print("MAX_ANGLE = "); break;
        case 'c': MIN_POSITION = nuevoValor; Serial.print("MIN_POSITION = "); break;
        case 'd': MAX_POSITION = nuevoValor; Serial.print("MAX_POSITION = "); break;
        case 'e': ZONA_ALERTA = nuevoValor; Serial.print("ZONA_ALERTA = "); break;
        case 'f': KP_POS = nuevoValor; Serial.print("KP_POS = "); break;
      }
      Serial.println(nuevoValor, 2);
    } else {
      Serial.println("Opcion no valida.");
    }
  }
}

// ================= RUTINAS DE MOVIMIENTO =================

void llevarACentro() {
  Serial.println("Moviendo carro a posicion central (45 cm)...");
  motorSpeed = 280;
  
  while (true) {
    leerSensoresCada2ms();
    updateDisplay();
    
    if (posicionCm < 44.0) {
      motorAdelante();
      motorDirection = "ADELANTE";
    } else if (posicionCm > 46.0) {
      motorAtras();
      motorDirection = "ATRAS";
    } else {
      break;
    }
  }
  
  motorFreno();
  motorDirection = "DETENIDO";
  Serial.print("Carro en posicion: ");
  Serial.print(posicionCm, 1);
  Serial.println(" cm");
}

void iniciarRutina() {
  Serial.println();
  Serial.println("--- INICIANDO RUTINA ---");
  llevarACentro();
  Serial.println("Estabilizando... espere 3 segundos.");
  delay(3000);
  
  // Resetear estados de control
  initialPositionReached = false;
  motorEnabled = true;
  error = 0.0;
  error_1 = 0.0;
  error_2 = 0.0;
  sistemaIniciado = true;
  
  Serial.println("Control activo. Envie 's' por Serial para detener.");
  Serial.println();
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);
  delay(2000); // Esperar conexión del monitor serial

  // Configuración de pines del motor
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  // Inicialización I2C y sensores
  I2Cuno.begin(SDA_PIN1, SCL_PIN1);
  I2Cdos.begin(SDA_PIN2, SCL_PIN2);
  
  as5600_1.resetPosition();
  as5600_2.resetPosition();
  as5600_2.resetCumulativePosition();

  // Inicializa OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Error OLED");
    while (1);
  }

  // Configurar PWM
  ledcSetup(0, PWM_FREQ, PWM_RES);
  ledcAttachPin(R_PWM_PIN, 0);
  ledcSetup(1, PWM_FREQ, PWM_RES);
  ledcAttachPin(L_PWM_PIN, 1);

  // ================= CONFIGURACION DE LA RED NEURONAL =================
  
  // Crear e inicializar arrays de entrada/salida
  XI.NewArray_2D(1, 3);
  XI.ToinitializeArray_2D(0.0);
  D.NewArray_2D(1, 1);
  D.ToinitializeArray_2D(0.0);
  
  byte XC = XI.GetColumns();
  byte DF = D.GetRows();
  byte XF = XI.GetRows();

  byte L = 3; // Número de capas
  EST.NewArray_2D(1, L);
  EST.ToinitializeArray_2D(0.0);
  float NC = EST.GetColumns();
  
  float V[] = {(float)XC, 3.0, (float)DF}; 
  for (byte i = 0; i < NC; i++) {
    EST.WriteArray_2D(0, i, V[i]);
  }

  FAC_STR.CharNewArray_1D(L);
  const char *myStrings[] = {"logsig", "logsig", "poslin_lim"};
  for (byte i = 0; i < L; i++) {
    FAC_STR.CharWriteArray_1D(i, (char*)myStrings[i]);
  }

  net.dposlin_limits(1, 0);
  net.LearningRate = 0.00;
  net.FEED_FORWARD_NET(EST, FAC_STR);
  
  posicion.NewArray_2D(1, 1);
  posicion.ToinitializeArray_2D(0.0);
  net.LearningRate = 0.4; // 0.025 en el original comentado
}

// ================= LOOP PRINCIPAL =================

void loop() {
  
  // ---- RAMA MENU: esperando órdenes del usuario ----
  if (!sistemaIniciado) {
    if (!menuMostrado) {
      mostrarMenu();
    }
    
    if (Serial.available()) {
      char cmd = Serial.read();
      while (Serial.available()) { Serial.read(); } // Limpiar buffer
      
      switch (cmd) {
        case '1':
          calibrarAngulo();
          menuMostrado = false;
          break;
        case '2':
          calibrarPosicion();
          menuMostrado = false;
          break;
        case '3':
          iniciarRutina();
          break;
        case '4':
          configurarParametros();
          menuMostrado = false;
          break;
      }
    }
    
    // Mantener display activo en modo menú
    leerSensoresCada2ms();
    updateDisplay();
    return; // No ejecutar control mientras estamos en menú
  }
  
  // ---- RAMA CONTROL: sistema en ejecución ----
  
  // Verificar parada por Serial
  if (Serial.available()) {
    char cmd = Serial.read();
    while (Serial.available()) { Serial.read(); }
    if (cmd == 's' || cmd == 'S') {
      Serial.println();
      Serial.println(">>> Sistema detenido por usuario. <<<");
      motorFreno();
      motorDirection = "DETENIDO";
      llevarACentro();
      sistemaIniciado = false;
      menuMostrado = false;
      initialPositionReached = false;
      return;
    }
  }
  
  // 1. Leer sensores
  leerSensoresCada2ms();

  // 2. Verificar limites de posición (protección de carril)
  if (posicionCm <= MIN_POSITION || posicionCm >= MAX_POSITION) {
    Serial.println();
    Serial.println(">>> Posicion fuera de rango! Recentrando... <<<");
    motorFreno();
    motorDirection = "DETENIDO";
    llevarACentro();
    Serial.println("Estabilizando... espere 3 segundos.");
    delay(3000);
    initialPositionReached = false;
    motorEnabled = true;
    error = 0.0;
    error_1 = 0.0;
    error_2 = 0.0;
    Serial.println("Reiniciando control automaticamente.");
    Serial.println();
    return;
  }

  // 3. Verificar limites de ángulo (solo en control principal)
  if (initialPositionReached && (anguloSensor1 < MIN_ANGLE || anguloSensor1 > MAX_ANGLE)) {
    motorEnabled = false;
    motorFreno();
    motorDirection = "DETENIDO";
  } else {
    motorEnabled = true;
  }

  // 4. Control solo si está habilitado
  if (motorEnabled) {
    
    // 5. Fase inicial: levantar péndulo (swing-up)
    if (!initialPositionReached) {
      // Factor de posición: reducir fuerza de swing-up cerca de los límites
      float distFromCenter = abs(posicionCm - posicionSetpoint);
      float maxDist = (MAX_POSITION - MIN_POSITION) / 2.0;
      float posFactor = constrain(1.0 - (distFromCenter / maxDist), 0.1, 1.0);
      
      if (anguloSensor1 > 0 && anguloSensor1 < MIN_ANGLE) {
        front = MIN_ANGLE - anguloSensor1;
        front = map(front, MIN_ANGLE, 0, 1, 0);
        motorSpeed = 500 * front * posFactor;
        motorAtras();
      } 
      else if (anguloSensor1 > MAX_ANGLE && anguloSensor1 < 360) {
        back = anguloSensor1 - MAX_ANGLE;
        back = map(back, MAX_ANGLE, 0, 1, 0);
        motorSpeed = 500 * back * posFactor;
        motorAdelante();
      }
      else {
        initialPositionReached = true; 
      }
    } 
    // 6. Control Principal (Red Neuronal + lazo de posición)
    else {
      // Lazo maestro de posición: corrige el setpoint de ángulo
      float posError = posicionSetpoint - posicionCm;
      float posCorrection = KP_POS * posError;
      posCorrection = constrain(posCorrection, -MAX_POS_ADJUST, MAX_POS_ADJUST);
      float effectiveSetpoint = ANGLE_SETPOINT + posCorrection;
      
      motorSpeed = angleControl(anguloSensor1, effectiveSetpoint);
      motorSpeed = map(motorSpeed, 0, 1000, 200, 800);

      error_2 = error_1;
      error_1 = error;

      // Aplicar dirección
      if (error > 0.01) {
        motorAdelante();
        motorDirection = "ADELANTE";
      } else if (error < -0.01) {
        motorAtras();
        motorDirection = "ATRAS";
      } else {
        motorFreno();
        motorDirection = "DETENIDO";
      }
    }
  }

  // 7. Actualizar pantalla
  updateDisplay();
  
  // 8. Mostrar estado por serial
  printSystemStatus();
  
  delay(10);
}