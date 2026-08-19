#include "motores_node.hpp"
#include <Arduino.h>


const int freq = 5000;      
const int resolution = 8;   
const int dutyCycle = 178;

const int dutyTracao  = 178;
const int dutyDirecao = 178;
const unsigned long TEMPO_ESTERCO_MS = 250;


volatile char comandoCamera = 'S'; 

void inicializarMotores(void)
{
  //ABRE UM CANAL PWM
  ledcSetup(CH_ESQ, freq, resolution);   ledcSetup(CH_DIR, freq, resolution);
  ledcSetup(CH_FR, freq, resolution);   ledcSetup(CH_TR, freq, resolution);

  //VINCULA O CANAL PWN COM O PINO
  ledcAttachPin(PIN_ESQ, CH_ESQ); ledcAttachPin(PIN_DIR, CH_DIR);
  ledcAttachPin(PIN_FR, CH_FR);  ledcAttachPin(PIN_TR, CH_TR);
}

void acionarPonte(int canalA, int canalB, int duty)
{
    if(duty > 0){
        ledcWrite(canalB, 0);       // zera o oposto antes
        ledcWrite(canalA, duty);    
    }

    else if(duty < 0){
        ledcWrite(canalA, 0);
        ledcWrite(canalB, -duty);
    }

    //parar
    else{
        ledcWrite(canalA, 0);
        ledcWrite(canalB, 0);
    }
}

void pararMotores() {
    acionarPonte(CH_FR, CH_TR, 0);
    acionarPonte(CH_ESQ, CH_DIR, 0);
}

void aplicarComando(char cmd)
{
    static char ultimoCmd = 'S';
    static unsigned long inicioEsterco = 0;

    if (cmd != ultimoCmd) {
        ultimoCmd = cmd;
        inicioEsterco = millis();
    }

    // --- tração: ligada em qualquer comando de movimento ---
    if (cmd == 'F' || cmd == 'L' || cmd == 'R') {
        acionarPonte(CH_FR, CH_TR, dutyTracao);
    } 
    else {
        acionarPonte(CH_FR, CH_TR, 0);// 'S' e qualquer byte estranho
    }

    // --- direção: pulsada, para não morrer no batente ---
    const unsigned long CICLO = TEMPO_ESTERCO_MS * 2;
    bool faseAtiva = ((millis() - inicioEsterco) % CICLO) < TEMPO_ESTERCO_MS;

    if (!faseAtiva) {
        acionarPonte(CH_ESQ, CH_DIR, 0);
    } 
    else if (cmd == 'L') {
        acionarPonte(CH_ESQ, CH_DIR,  dutyDirecao);
    } 
    else if (cmd == 'R') {
        acionarPonte(CH_ESQ, CH_DIR, -dutyDirecao);
    } 
    else {
        acionarPonte(CH_ESQ, CH_DIR, 0);    // 'F' = esterço solto
    }
}
