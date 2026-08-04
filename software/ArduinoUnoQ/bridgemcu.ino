#include <Arduino_RouterBridge.h>

void setup() {
  Serial.begin(115200); 
  
  Serial1.begin(115200);
  
  Bridge.begin();
  Bridge.provide("processa_direcao", processa_direcao);
  
  pinMode(LED_BUILTIN, OUTPUT);
}

void processa_direcao(String direcao) {
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.print(direcao); 
  Serial.flush();
  
  Serial1.print(direcao);
  Serial1.flush();
  
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
}
