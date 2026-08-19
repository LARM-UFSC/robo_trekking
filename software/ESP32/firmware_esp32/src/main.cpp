#include <Arduino.h>
#include "ultrassom_node.hpp"
#include "../include/mpu_node.hpp"
#include "motores_node.hpp"

#define RXD2 16   
#define TXD2 14   // Livre


TaskHandle_t TaskSensoresHandle;


void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial2.setRxBufferSize(1024);
  

  pinMode(LED_AZUL, OUTPUT);
  digitalWrite(LED_AZUL, LOW);

  inicializarMotores();
  pararMotores(); 

  xTaskCreatePinnedToCore(TaskSensores, "TaskSensores", 4096, NULL, 1, &TaskSensoresHandle, 0);
  Serial.println("-NP-");

  InicializarMPU();
}

void loop() {

  SinalVidaSensores();
  aplicarComando(comandoCamera);
  delay(15); 

  LeituraMPU();
}