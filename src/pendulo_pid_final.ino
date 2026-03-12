// ==============================
// Librerías
// ==============================
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AS5600.h>
#include <Arduino.h>

// ==============================
// Configuración de Hardware
// ==============================
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

// ==============================
// Variables del sistema
// ==============================
volatile float posicionCm = 0.0;
volatile float posicionSetpoint = 45.0;
int motorSpeed = 0;
bool motorEnabled = true;
String motorDirection = "DETENIDO";

// Comunicación entre núcleos
SemaphoreHandle_t xMutex = NULL;
volatile int rev = 0;
volatile long posicionAcumulada = 0;
const float CM_POR_GRADO = 0.0115780;

// ==============================
// Sensores y Display
// ==============================
TwoWire I2Cuno = TwoWire(0);
TwoWire I2Cdos = TwoWire(1);
AS5600 as5600_1(&I2Cuno);
AS5600 as5600_2(&I2Cdos); 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cuno);

// Ángulos
float anguloSensor1 = 0.0;
float anguloSensor2 = 0.0;
float anguloAnterior2 = 0.0;
long vueltas = 0;

// ==============================
// PID Ángulo (Maestro)
// ==============================
float angleKp = 17;
float angleKi = 0.8;
float angleKd = 0.0255;
float angleIntegral = 0.0;
float anglePrevError = 0.0;
const float ANGLE_SETPOINT = 180.0;
const float INITIAL_ANGLE_SP = 45.5;
bool initialPositionReached = false;

// ==============================
// PID Posición (Esclavo)
// ==============================
float posKp = 4.0;
float posKi = 50;
float posKd = 1.6;
float posIntegral = 0.0;
float posPrevError = 0.0;

// ==============================
// Límites del sistema
// ==============================
const float MIN_ANGLE = 165.0;
const float MAX_ANGLE = 195.0;
const float MIN_POSITION = 10.0;
const float MAX_POSITION = 85.0;
const float MAX_POS_ADJUST = 2.0;
const float ZONA_ALERTA = 5;

// ==============================
// Tarea de lectura del sensor 2 (Core 0)
// ==============================
void TaskSensor2(void *pvParameters) {
  static uint32_t lastTime = 0;
  float anguloAnterior2 = as5600_2.getCumulativePosition();

  while (1) {
    if (millis() - lastTime >= 10) {
      lastTime = millis();

      if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        float currentAngle = as5600_2.getCumulativePosition();
        rev = as5600_2.getRevolutions();

        float delta = currentAngle - anguloAnterior2;
        if (delta < -2000) vueltas++;
        else if (delta > 2000) vueltas--;

        posicionAcumulada = vueltas * 360 + map(currentAngle, 0, 4095, 0, 360);
        posicionCm = posicionAcumulada * CM_POR_GRADO;

        anguloAnterior2 = currentAngle;
        xSemaphoreGive(xMutex);
      }
    }
    vTaskDelay(1);
  }
}

// ==============================
// SETUP
// ==============================
void setup() {
  Serial.begin(115200);

  // Inicialización I2C y sensores
  I2Cuno.begin(SDA_PIN1, SCL_PIN1);
  I2Cdos.begin(SDA_PIN2, SCL_PIN2);
  as5600_1.resetPosition();
  as5600_2.resetPosition();
  as5600_2.resetCumulativePosition();

  // Configuración de pines del motor
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  // Inicialización de pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Error OLED");
    while (1);
  }

  // Configuración PWM
  ledcAttach(R_PWM_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(L_PWM_PIN, PWM_FREQ, PWM_RES);

  // Crear mutex
  xMutex = xSemaphoreCreateMutex();

  // Iniciar tarea en Core 0
  xTaskCreatePinnedToCore(
    TaskSensor2,
    "TaskSensor2",
    10000,
    NULL,
    1,
    NULL,
    0
  );
}

// ==============================
// Control PID Ángulo (Maestro)
// ==============================
float angleControl(float currentAngle) {
  float dt = 0.01;
  float error = ANGLE_SETPOINT - currentAngle;

  // Compensación por proximidad a bordes
  float positionFactor = 1.0;
  if (posicionCm < (MIN_POSITION + ZONA_ALERTA)) {
    positionFactor = map(posicionCm, MIN_POSITION, MIN_POSITION + ZONA_ALERTA, 2.0, 1.0);
  } else if (posicionCm > (MAX_POSITION - ZONA_ALERTA)) {
    positionFactor = map(posicionCm, MAX_POSITION - ZONA_ALERTA, MAX_POSITION, 1.0, 2.0);
  }

  float P = angleKp * error * positionFactor;

  if (abs(error) > 0.25) {
    angleIntegral += error * dt;
    if (positionFactor > 1.2) angleIntegral *= 0.8;
    angleIntegral = constrain(angleIntegral, -250.0, 250.0);
  }
  float I = angleKi * angleIntegral;

  float derivative = (error - anglePrevError) / dt;
  float D = angleKd * derivative * (positionFactor > 1.2 ? 1.5 : 1.0);

  anglePrevError = error;

  float posAdjustment = P + I + D;
  posAdjustment = constrain(posAdjustment, -MAX_POS_ADJUST, MAX_POS_ADJUST);
  return posAdjustment;
}

// ==============================
// Control PID Posición (Esclavo)
// ==============================
int positionControl(float currentPos, float setPoint) {
  float dt = 0.01;
  float error = setPoint - currentPos;

  float P = posKp * error;

  if (abs(error) > 0.1) {
    posIntegral += error * dt;
    posIntegral = constrain(posIntegral, -18.0, 18.0);
  }
  float I = posKi * posIntegral;

  float derivative = (error - posPrevError) / dt;
  float D = posKd * derivative;

  posPrevError = error;

  int output = abs(P + I + D);
  output = constrain(output, 425, 800);
  return output;
}

// ==============================
// Mostrar estado por serial
// ==============================
void printSystemStatus() {
  Serial.print("Angulo: ");
  Serial.print(anguloSensor1, 1);
  Serial.print("° | Pos: ");
  Serial.print(posicionCm, 1);
  Serial.print("cm | SP Pos: ");
  Serial.print(posicionSetpoint, 1);
  Serial.print("cm | PWM: ");
  Serial.print(motorSpeed);
  Serial.print(" | Dir: ");
  Serial.print(motorDirection);
  Serial.print(" | Estado: ");

  if (!motorEnabled) Serial.println("ALERTA");
  else if (!initialPositionReached) Serial.println("INICIAL");
  else Serial.println("VERTICAL");
}

// ==============================
// LOOP
// ==============================
void loop() {
  // Lectura de ángulo del péndulo
  anguloSensor1 = map(as5600_1.readAngle(), 0, 4095, 3600, 0) / 10.0;

  // Seguridad: desactiva motor si se exceden límites
  if (anguloSensor1 < MIN_ANGLE || anguloSensor1 > MAX_ANGLE) {
    motorEnabled = false;
    motorFreno();
    motorDirection = "DETENIDO";
  } else {
    motorEnabled = true;
  }

  if (motorEnabled) {
    // Fase inicial: llevar a posición inicial
    if (!initialPositionReached) {
      if (abs(anguloSensor1 - INITIAL_ANGLE_SP) < 0.5) {
        initialPositionReached = true;
        angleIntegral = 0;
      }
    }

    // Leer posición actual desde sensor 2
    float currentPos = 0.0;
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      currentPos = posicionCm;
      xSemaphoreGive(xMutex);
    }

    // Control maestro: ajustar setpoint de posición
    float posAdjustment = angleControl(anguloSensor1);
    posicionSetpoint += posAdjustment;
    posicionSetpoint = constrain(posicionSetpoint, MIN_POSITION, MAX_POSITION);

    // Control esclavo: calcular PWM
    motorSpeed = positionControl(currentPos, posicionSetpoint);

    // Dirección del motor
    float posError = posicionSetpoint - currentPos;
    if (posError > 0.5) {
      motorAdelante();
      motorDirection = "ADELANTE";
    } else if (posError < -0.5) {
      motorAtras();
      motorDirection = "ATRAS";
    } else {
      motorFreno();
      motorDirection = "DETENIDO";
    }
  }

  // Actualizar pantalla OLED y consola
  updateDisplay();
  printSystemStatus();
  delay(10);
}

// ==============================
// Pantalla OLED
// ==============================
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Angulo: ");
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print(anguloSensor1, 1);
  display.print("°");

  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Posicion: ");
  display.setTextSize(2);
  display.setCursor(0, 40);
  display.print(posicionCm, 1);
  display.print("cm");

  display.setTextSize(1);
  display.setCursor(70, 0);
  display.print("PWM: ");
  display.print(motorSpeed);
  display.setCursor(70, 10);
  display.print("Dir: ");
  display.print(motorDirection);

  display.setCursor(70, 30);
  if (!motorEnabled) display.print("ALERTA");
  else if (!initialPositionReached) display.print("INICIAL");
  else display.print("VERTICAL");

  if ((posicionCm < (MIN_POSITION + ZONA_ALERTA)) || (posicionCm > (MAX_POSITION - ZONA_ALERTA))) {
    display.setCursor(70, 20);
    display.print("!ZONA CRIT!");
  }

  display.display();
}

// ==============================
// Control del Motor
// ==============================
void motorAdelante() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(L_PWM_PIN, 0);
  ledcWrite(R_PWM_PIN, motorSpeed);
}

void motorAtras() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(R_PWM_PIN, 0);
  ledcWrite(L_PWM_PIN, motorSpeed);
}

void motorFreno() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(R_PWM_PIN, 0);
  ledcWrite(L_PWM_PIN, 0);
}
