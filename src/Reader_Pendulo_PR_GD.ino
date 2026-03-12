#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AS5600.h>
#include <Arduino.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define SDA_PIN1 21  // Beispiel: GPIO 21 für SDA
#define SCL_PIN1 22  // Beispiel: GPIO 22 für SCL
#define SDA_PIN2 18
#define SCL_PIN2 19
#define PWM_FREQ 25000    // Frecuencia en Hz
#define PWM_RES 10

 int R_EN_PIN = 26;
 int L_EN_PIN = 27;
 int R_PWM_PIN = 14; // Asegúrate que este pin puede ser PWM
 int L_PWM_PIN = 12; // Asegúrate que este pin puede ser PWM
   
// Variable global para almacenar el valor acumulado
volatile float posicionCm = 0.0;
volatile float setPoint = 0.0;  // Posición objetivo en cm
String inputString = "";        // Almacena datos del serial
bool newData = false;           // Flag para nuevo comando
int motorSpeed = 0;

// Variables compartidas entre núcleos (usa un semáforo para evitar conflictos)
SemaphoreHandle_t xMutex = NULL;  // Mutex para acceso seguro a variables
volatile int rev = 0;             // Variable compartida
volatile long posicionAcumulada = 0;
const float CM_POR_GRADO = 0.0115780; //

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
// Tarea para Core 0: Lectura del Sensor 2 (AS5600)
// ==============================================
void TaskSensor2(void *pvParameters) {
  static uint32_t lastTime = 0;
  int anguloAnterior2 = as5600_2.getCumulativePosition();

  while (1) {
    // Ejecuta cada 10 ms (misma lógica original)
    if (millis() - lastTime >= 10) {
      lastTime = millis();

      // Bloquea el mutex para acceso seguro
      if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        // Lectura del sensor 2
        int currentAngle = as5600_2.getCumulativePosition();
        rev = as5600_2.getRevolutions();

        // Cálculo de posición acumulada (si es necesario)
        int delta = currentAngle - anguloAnterior2;
        if (delta < -2000) vueltas++;
        else if (delta > 2000) vueltas--;
        posicionAcumulada = vueltas * 360 + map(currentAngle, 0, 4095, 0, 360);
        posicionCm = posicionAcumulada * CM_POR_GRADO; // Conversión a cm

        anguloAnterior2 = currentAngle;
        xSemaphoreGive(xMutex);  // Libera el mutex
      }
    }
    vTaskDelay(1);  // Libera el CPU brevemente
  }
}

// ==============================================
// Configuración Inicial
// ==============================================
void setup() {
  Serial.begin(115200);
   int anguloSensor1;
  // Inicializa I2C y sensores (igual que antes)
  I2Cuno.begin(SDA_PIN1, SCL_PIN1);
  I2Cdos.begin(SDA_PIN2, SCL_PIN2);
  as5600_1.resetPosition();
  as5600_2.resetPosition();
  as5600_2.resetCumulativePosition();
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);

  // Inicializa OLED (igual que antes)
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Error OLED");
    while (1);
  }

  // Habilitar puente H
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  // Configurar PWM
  ledcAttach(R_PWM_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(L_PWM_PIN, PWM_FREQ, PWM_RES);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);


  // Crea el mutex para variables compartidas
  xMutex = xSemaphoreCreateMutex();

  // Crea la tarea para el Core 0 (Sensor 2)
  xTaskCreatePinnedToCore(
    TaskSensor2,   // Función de la tarea
    "TaskSensor2", // Nombre
    10000,         // Tamaño de pila
    NULL,          // Parámetros
    1,             // Prioridad
    NULL,          // Manejador
    0              // Núcleo (Core 0)
  );
}

// ==============================================
// Loop Principal (se ejecuta en Core 1)
// ==============================================
void loop() {
  // Lectura del sensor 1 (Core 1)
  anguloSensor1 = map(as5600_1.readAngle(), 0, 4095, 360, 0); 
  float currentPosition = 0.0;

  // Variables adicionales para PID
  float Kp = 6;    // Ganancia proporcional
  float Ki = 60.0;    // Ganancia integral
  float Kd = 1.6;    // Ganancia derivativa
  
  float integral = 0.0;  // Término integral
  float prevError = 0.0; // Error anterior
  float dt = 0.01;       // Intervalo de tiempo (10ms)

  // Calcular el control PID
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      currentPosition = posicionCm;
      xSemaphoreGive(xMutex);
  }
  
  // Calcular el error
  float error = setPoint - currentPosition;
  
  // Término proporcional
  float P = Kp * error;
  
  // Término integral (acumular solo si hay error significativo)
  if (abs(error) > 0.1) { // Evitar integración de errores pequeños
      integral += error * dt;
  }
  float I = Ki * integral;
  
  // Término derivativo
  float derivative = (error - prevError) / dt;
  float D = Kd * derivative;
  
  // Salida del controlador PID
  motorSpeed = abs(P + I + D);
  
  // Limitar la velocidad del motor
  int minSpeed = 220;
  int maxSpeed = 500;
  motorSpeed = constrain(motorSpeed, minSpeed, maxSpeed);
  
  // Almacenar valores actuales para la próxima iteración
  prevError = error;

  // Reiniciar término integral si estamos cerca del punto de ajuste
  if (abs(error) < 0.05) {
      integral = 0;
  }

  // Dirección del motor
  if (error  > 0.02) { // Mover motor hacia adelante si el error es mayor que 0.5 cm
    motorAdelante();
    ledcWrite(R_PWM_PIN, motorSpeed); // Ajustar velocidad
    Serial.print("Motor Adelante, Velocidad: ");
    Serial.println(motorSpeed);
    Serial.println(error);
  } else if (error < -0.02) { // Mover motor hacia atrás si el error es menor que -0.5 cm
    motorAtras();
    ledcWrite(L_PWM_PIN, motorSpeed); // Ajustar velocidad
    Serial.print("Motor Atrás, Velocidad: ");
    Serial.println(motorSpeed);
     Serial.print("Motor Atrás, Velocidad: ");
     Serial.println(error);
  } else {
    // Motor detenido si el error es menor que 2.5 cm
    motorFreno();
    Serial.println("Motor Detenido");
     Serial.println(error);
  }

  // Actualiza la pantalla y controla el motor (Core 1)
  actualizarPantalla();

  // Imprime valores (usando mutex para variables compartidas)
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print("Revoluciones: ");
    Serial.println(rev);
    Serial.print("Posición(cm): ");
    Serial.println(posicionCm);
    xSemaphoreGive(xMutex);
  }

    // Leer comandos del serial
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      newData = true;
      break;
    }
    inputString += inChar;
  }

  // Procesar comando si hay datos nuevos
  if (newData) {
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      setPoint = inputString.toFloat(); // Convertir a float
      Serial.print("Nuevo SetPoint: ");
      Serial.println(setPoint);
      inputString = "";
      newData = false;
      xSemaphoreGive(xMutex);
    }
  }
  
  delay(10);  // Pequeña pausa para no saturar
  }

void actualizarPantalla() {
  display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Posición (cm): ");
  display.setCursor(0, 10);
  display.setTextSize(2);
  display.print(posicionCm);
  display.setCursor(0,30);
  display.setTextSize(1);
  display.print("angulo: ");
  display.setTextSize(2);
  display.setCursor(0, 40);
  
  display.print(anguloSensor1);
  display.display();
}

// Función para mover el motor en sentido horario
void motorAdelante() { // velocidad de 0 a 255 (para resolución de 8 bits)


  digitalWrite(R_EN_PIN, HIGH); // Habilitar driver derecho
  digitalWrite(L_EN_PIN, HIGH); // Habilitar driver izquierdo (o mantenerlo habilitado)
  
  ledcWrite(L_PWM_PIN, 0);         // Usa el canal, no el pin
  ledcWrite(R_PWM_PIN, motorSpeed); // Canal derecho
  Serial.print("Motor Adelante, Velocidad: ");
  Serial.println(motorSpeed);
}

// Función para mover el motor en sentido antihorario
void motorAtras() { // velocidad de 0 a 255


  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  ledcWrite(R_PWM_PIN, 0);         // Usa el canal
  ledcWrite(L_PWM_PIN, motorSpeed); // Canal izquierdo
  Serial.print("Motor Atrás, Velocidad: ");
  Serial.println(motorSpeed);
}

// Función para frenar el motor (freno dinámico)
void motorFreno() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  ledcWrite(R_EN_PIN, 0); // Poner ambos PWM a 0 podría causar freno dinámico
  ledcWrite(L_EN_PIN, 0); // o HIGH, verifica la hoja de datos. LOW es común.
  Serial.println("Motor Frenado");
  // Para un frenado más fuerte, podrías poner ledcWrite(R_PWM_CHANNEL, 255) y ledcWrite(L_PWM_CHANNEL, 255)
  // si tu módulo lo soporta de esa manera, pero es menos común.
  // Lo más seguro es R_PWM=LOW, L_PWM=LOW con EN=HIGH
}

// Función para parada libre (coasting)
void motorParadaLibre() {
  digitalWrite(R_EN_PIN, LOW);
  digitalWrite(L_EN_PIN, LOW);
  // El estado de los PWM es irrelevante aquí
  Serial.println("Motor Parada Libre (Coasting)");
}
