#include <Arduino.h>
#include <NewPing.h>


#define RXD2 16   
#define TXD2 14   // Livre
#define LED_AZUL 2

volatile char comandoCamera = 'S'; 

// ULTRASSON - CONFIGURAÇÃO NEWPING
const int PIN_TRIGGER = 26;   
const int PIN_ECHO_1  = 25;   // S1: FRENTE DIREITA
const int PIN_ECHO_2  = 27;   // S2: FRENTE ESQUERDA
const int PIN_ECHO_3  = 33;   // S3: ATRÁS DIREITA
const int PIN_ECHO_4  = 32;   // S4: ATRÁS ESQUERDA       

#define MAX_DISTANCE 400 

NewPing sonar1(PIN_TRIGGER, PIN_ECHO_1, MAX_DISTANCE);
NewPing sonar2(PIN_TRIGGER, PIN_ECHO_2, MAX_DISTANCE);
NewPing sonar3(PIN_TRIGGER, PIN_ECHO_3, MAX_DISTANCE);
NewPing sonar4(PIN_TRIGGER, PIN_ECHO_4, MAX_DISTANCE);

const long intervaloCicloTotal = 100; 

volatile float dist_1 = 400.0f, dist_2 = 400.0f, dist_3 = 400.0f, dist_4 = 400.0f;
TaskHandle_t TaskSensoresHandle;

// MOTORES
#define PIN_ESQ 22
#define PIN_DIR 23
#define PIN_FR 19
#define PIN_TR 21

const int freq = 5000;      
const int resolution = 8;   
const int dutyCycle = 178;

void pararMotores() {
  ledcWrite(0, 0); 
  ledcWrite(1, 0); 
  ledcWrite(2, 0); 
  ledcWrite(3, 0);
}

float processarLeituraNewPing(unsigned int ping_us) {
  if (ping_us == 0) return 400.0f;
  float cm = (float)ping_us / US_ROUNDTRIP_CM;
  if (cm < 4.0f) return 400.0f;
  return cm;
}

void TaskSensores(void * pvParameters) {
  (void) pvParameters;
  Serial.print("NewPing Ativo no Núcleo: ");
  Serial.println(xPortGetCoreID());

  for(;;) {
    
    dist_1 = processarLeituraNewPing(sonar1.ping());
    delay(30);

    dist_2 = processarLeituraNewPing(sonar2.ping());
    delay(30);

    dist_3 = processarLeituraNewPing(sonar3.ping());
    delay(30);

    dist_4 = processarLeituraNewPing(sonar4.ping());
    
    
    vTaskDelay(pdMS_TO_TICKS(intervaloCicloTotal));
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial2.setRxBufferSize(1024);
  
  ledcSetup(0, freq, resolution);   ledcSetup(1, freq, resolution);
  ledcSetup(2, freq, resolution);   ledcSetup(3, freq, resolution);

  ledcAttachPin(PIN_ESQ, 0); ledcAttachPin(PIN_DIR, 1);
  ledcAttachPin(PIN_FR, 2);  ledcAttachPin(PIN_TR, 3);
  
  pinMode(LED_AZUL, OUTPUT);
  digitalWrite(LED_AZUL, LOW);

  xTaskCreatePinnedToCore(TaskSensores, "TaskSensores", 4096, NULL, 1, &TaskSensoresHandle, 0);
  Serial.println("-NP-");
}

void loop() {
  if (Serial2.available() > 0) {
    comandoCamera = Serial2.read(); 
    digitalWrite(LED_AZUL, HIGH);
  }

  float num_camera = 2.0f; 
  if (comandoCamera == 'L') num_camera = -1.0f;
  else if (comandoCamera == 'F') num_camera = 0.0f;
  else if (comandoCamera == 'R') num_camera = 1.0f;

  Serial.print("Camera: ");
  Serial.print(comandoCamera);
  Serial.print(" | ValorCamera: ");
  Serial.print(num_camera, 1);
  Serial.print(" | Sensor1: ");
  Serial.print(dist_1, 1);
  Serial.print("cm | Sensor2: ");
  Serial.print(dist_2, 1);
  Serial.print("cm | Sensor3: ");
  Serial.print(dist_3, 1);
  Serial.print("cm | Sensor4: ");
  Serial.print(dist_4, 1);
  Serial.println("cm");

  digitalWrite(LED_AZUL, LOW);
  delay(15); 
}