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
const float MIN_ANGLE = ANGLE_SETPOINT - 45.0; // 135.0
const float MAX_ANGLE = ANGLE_SETPOINT + 45.0; // 225.0

const float MIN_POSITION = 10.0;
const float MAX_POSITION = 85.0;
const float MAX_POS_ADJUST = 2.0;
const float ZONA_ALERTA = 5.0;

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
    
    // --- Sensor 1 (ángulo) ---
    anguloSensor1 = map(as5600_1.readAngle(), 0, 4095, 3600, 0) / 10.0;
    
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

float angleControl(float currentAngle) {
  error = ANGLE_SETPOINT - currentAngle;
  
  posicion.WriteArray_2D(0, 0, currentAngle / 1000.00);
  XI.WriteArray_2D(0, 0, error);
  XI.WriteArray_2D(0, 1, error - error_1);
  XI.WriteArray_2D(0, 2, error_1 - error_2);
  
  net.NORMALIZE_DATA(XI, 1000.00);
  D.WriteArray_2D(0, 0, ANGLE_SETPOINT / 1000.00);
  
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
  Serial.print("error: "); Serial.print(error, 2);
  Serial.print(" | error1: "); Serial.print(error_1, 2);
  Serial.print(" | error2: "); Serial.print(error_2, 2);
  Serial.print(" | PWM: "); Serial.print(motorSpeed);
  Serial.print(" | Output: "); Serial.print(Output, 2);
  Serial.print(" | back: "); Serial.print(back, 2);
  Serial.print(" | front: "); Serial.print(front, 2);
  Serial.print(" | anguloSP: "); Serial.print(anguloSP, 2);
  Serial.print(" | Estado: ");
  
  if (!motorEnabled) {
    Serial.println("Detenido");
  } else if (!initialPositionReached) {
    Serial.println("Control Inicial");
  } else {
    Serial.println("VERTICAL");
  }
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);

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

  // Llevar el péndulo al centro inicial
  while (posicionCm < 45.0) {
    leerSensoresCada2ms();
    motorSpeed = 280;
    motorAdelante();
    motorDirection = "ADELANTE";
  }
  
  motorFreno();
  delay(5000);
}

// ================= LOOP PRINCIPAL =================

void loop() {
  // 1. Leer ángulo actual con decimales
  leerSensoresCada2ms();

  // 2. Verificar limites de seguridad
  if (anguloSensor1 < MIN_ANGLE || anguloSensor1 > MAX_ANGLE) {
    motorEnabled = false;
    motorFreno();
    motorDirection = "DETENIDO";
  } else {
    motorEnabled = true;
  }

  // 3. Control solo si está habilitado
  if (motorEnabled) {
    
    // 4. Fase inicial: levantar péndulo
    if (!initialPositionReached) {
      if (anguloSensor1 > 0 && anguloSensor1 < MIN_ANGLE) {
        front = MIN_ANGLE - anguloSensor1;
        front = map(front, MIN_ANGLE, 0, 1, 0);
        motorSpeed = 500 * front;
        motorAtras();
      } 
      else if (anguloSensor1 > MAX_ANGLE && anguloSensor1 < 360) {
        back = anguloSensor1 - MAX_ANGLE;
        back = map(back, MAX_ANGLE, 0, 1, 0);
        motorSpeed = 500 * back;
        motorAdelante();
      }
      else {
        initialPositionReached = true; 
      }
    } 
    // 5. Control Principal
    else {
      float currentPos = posicionCm;
      
      // Control de posición con Red Neuronal
      motorSpeed = angleControl(anguloSensor1);
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

  // 10. Actualizar pantalla
  updateDisplay();
  
  // 11. Mostrar estado por serial
  printSystemStatus();
  
  delay(10);
}