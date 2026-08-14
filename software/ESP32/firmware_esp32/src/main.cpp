#include <Arduino.h>
#include "ultrassom_node.hpp"
#include "../include/mpu_node.hpp"


#define RXD2 16   
#define TXD2 14   // Livre

TaskHandle_t TaskSensoresHandle;

//MOTORES
#define PIN_ESQ 23
#define PIN_DIR 4
#define PIN_FR 13
#define PIN_TR 32

const int freq = 5000;      
const int resolution = 8;   
const int dutyCycle = 178;

void pararMotores() {
  ledcWrite(0, 0); 
  ledcWrite(1, 0); 
  ledcWrite(2, 0); 
  ledcWrite(3, 0);
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

  InicializarMPU();
}

void loop() {

  SinalVidaSensores();
  delay(15); 

  LeituraMPU();
}