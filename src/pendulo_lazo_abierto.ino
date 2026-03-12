#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AS5600.h>
#include <Arduino.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define SDA_PIN1 21
#define SCL_PIN1 22
#define SDA_PIN2 18
#define SCL_PIN2 19
#define PWM_FREQ 25000
#define PWM_RES 10
#define RUN_TIME 200  // Tiempo de operación en milisegundos (5 segundos)

// Pines del motor
int R_EN_PIN = 26;
int L_EN_PIN = 27;
int R_PWM_PIN = 14;
int L_PWM_PIN = 12;

// Variables globales
volatile float posicionCm = 0.0;
int motorSpeed = 800;
char motorDirection = 'S';  // 'F': adelante, 'B': atrás, 'S': detenido
bool systemActive = true;
unsigned long startTime = 0;
int anguloc;
// Sincronización entre núcleos
SemaphoreHandle_t xMutex = NULL;
volatile int rev = 0;
volatile long posicionAcumulada = 0;
const float CM_POR_GRADO = 0.0115780;

// Sensores y displays
TwoWire I2Cuno = TwoWire(0);
TwoWire I2Cdos = TwoWire(1);
AS5600 as5600_1(&I2Cuno);
AS5600 as5600_2(&I2Cdos); 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cuno);

// Variables de sensores
int anguloSensor1 = 0;
int anguloSensor2 = 0;
int anguloAnterior2 = 0;
long vueltas = 0;
// ==============================================
// Configuración Inicial
// ==============================================
void setup() {
  Serial.begin(115200);  // Para comunicación con PLX-DAQ
  delay(5000);
  I2Cuno.begin(SDA_PIN1, SCL_PIN1);
  I2Cdos.begin(SDA_PIN2, SCL_PIN2);
  as5600_1.setOffset(0);
  as5600_2.setOffset(0);
  as5600_1.resetPosition();
  as5600_2.resetPosition();
   as5600_1.resetCumulativePosition();
  as5600_2.resetCumulativePosition();
  
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }

  ledcAttach(R_PWM_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(L_PWM_PIN, PWM_FREQ, PWM_RES);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  xMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(
    TaskSensor2,
    "TaskSensor2",
    10000,
    NULL,
    1,
    NULL,
    0
  );

  // Configurar PLX-DAQ
  Serial.println("CLEARSHEET");         // Limpiar hoja de cálculo
  Serial.println("LABEL,Tiempo (ms),Posición (cm),Ángulo (°),Ángulo Conpensado (°),Velocidad Motor (°)"); // Encabezados
  
  startTime = millis();  // Iniciar el contador de tiempo
  motorAdelante(800);
}

// ==============================================
// Tarea para Core 0: Lectura del Sensor 2 (AS5600)
// ==============================================
void TaskSensor2(void *pvParameters) {
  static uint32_t lastTime = 0;
  int anguloAnterior2 = as5600_2.getCumulativePosition();

  while (1) {
    if (millis() - lastTime >= 1) {
      lastTime = millis();

      if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        int currentAngle = as5600_2.getCumulativePosition();
        rev = as5600_2.getRevolutions();

        int delta = currentAngle - anguloAnterior2;
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


// ==============================================
// Loop Principal (Core 1)
// ==============================================
void loop() {
  static uint32_t lastSampleTime = 0;
  const uint32_t sampleInterval =1; // 10 ms = 100 Hz
   
  // Verificar tiempo transcurrido
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - startTime;
  
  // Apagar sistema después de 5 segundos
  if (systemActive && elapsedTime >= RUN_TIME) {
    systemActive = false;
    motorFreno(); 
    motorSpeed =0;
   
  }
  // Lectura del sensor 1
  anguloSensor1 = map(as5600_1.readAngle(), 0, 4095,360, 0); 
  if (anguloSensor1>359){
  anguloc=0; } 
                
    
if (anguloSensor1<361&&anguloSensor1>180){
  anguloc=anguloSensor1-360;
  }
 else{
    anguloc=anguloSensor1;
    }      

  // Muestreo periódico para PLX-DAQ
  if (millis() - lastSampleTime >= sampleInterval) {
    lastSampleTime = millis();
    
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      // Enviar datos a Excel
      Serial.print("DATA,");
      Serial.print(elapsedTime);
      Serial.print(",");
      Serial.print(posicionCm, 3);
      Serial.print(",");
      Serial.print(anguloSensor1);
      Serial.print(",");
      Serial.print(anguloc);
      Serial.print(",");
      Serial.println(motorSpeed);  // Único println al final
             // velocidad motor
      xSemaphoreGive(xMutex);
    }
  }

  actualizarPantalla();
  
}

// ==============================================
// Funciones de control de motor
// ==============================================
void motorAdelante(int velocidad) {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(L_PWM_PIN, 0);
  ledcWrite(R_PWM_PIN, velocidad);
}

void motorAtras(int velocidad) {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(R_PWM_PIN, 0);
  ledcWrite(L_PWM_PIN, velocidad);
}

void motorFreno() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  ledcWrite(R_PWM_PIN, 0);
  ledcWrite(L_PWM_PIN, 0);
}

// ==============================================
// Actualización de pantalla OLED
// ==============================================
void actualizarPantalla() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Estado del sistema
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Estado: ");
  display.print(systemActive ? "ACTIVO" : "APAGADO");
  
  // Tiempo transcurrido
  display.setCursor(70, 0);
  display.print("T: ");
  display.print((millis() - startTime) / 1000.0, 1);
  display.print("s");
  
  // Posición
  display.setCursor(0, 15);
  display.print("Pos (cm):");
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(posicionCm, 1);
  
  // Ángulo
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Angulo:");
  display.setTextSize(2);
  display.setCursor(0, 55);
  display.print(anguloSensor1);
  display.print((char)247); // Símbolo de grados

  // Velocidad
  display.setTextSize(1);
  display.setCursor(70, 45);
  display.print("Vel:");
  display.setTextSize(2);
  display.setCursor(70, 55);
  display.print(motorSpeed);
  
  display.display();
}
