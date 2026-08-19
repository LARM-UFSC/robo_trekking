#include "ultrassom_node.hpp"



// ULTRASSON - CONFIGURAÇÃO NEWPING
const int PIN_TRIGGER = 26;   
const int PIN_ECHO_1  = 25;   // S1: FRENTE DIREITA
const int PIN_ECHO_2  = 27;   // S2: FRENTE ESQUERDA
const int PIN_ECHO_3  = 33;   // S3: ATRÁS DIREITA
const int PIN_ECHO_4  = 34;   // S4: ATRÁS ESQUERDA       

#define MAX_DISTANCE 400 

NewPing sonar1(PIN_TRIGGER, PIN_ECHO_1, MAX_DISTANCE);
NewPing sonar2(PIN_TRIGGER, PIN_ECHO_2, MAX_DISTANCE);
NewPing sonar3(PIN_TRIGGER, PIN_ECHO_3, MAX_DISTANCE);
NewPing sonar4(PIN_TRIGGER, PIN_ECHO_4, MAX_DISTANCE);

const long intervaloCicloTotal = 100; 

volatile float dist_1 = INVALID_DISTANCE, dist_2 = INVALID_DISTANCE, 
                dist_3 = INVALID_DISTANCE, dist_4 = INVALID_DISTANCE;


float processarLeituraNewPing(unsigned int ping_us) {
  if (ping_us == 0) return INVALID_DISTANCE;
  float cm = (float)ping_us / US_ROUNDTRIP_CM;
  if (cm < 4.0f) return INVALID_DISTANCE;
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

void SinalVidaSensores(void)
{
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
}
