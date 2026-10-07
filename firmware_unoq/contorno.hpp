#ifndef CONTORNO_HPP
#define CONTORNO_HPP

// contorna o cone assim que entra no modo contornar pelo ia_trekking.py
// utilizando os sensores ultrassom. contorna ate ver outro cone longe e aproxiomar novamente

int lado = 0; //decide lado
unsigned long pausa = 0; //definir tempo da pausa quando entra em modo contorno
unsigned long re = 0; //definir tempo da re

void reiniciarContorno(){
    pausa = 0;
    re = 0;
    lado = 0;
}

void passoContorno(int gradiente, int &vel, int &dir){
    unsigned long inicio = millis();
    vel = 0;
    dir = 0;

    if(pausa == 0) pausa = inicio + 2000; //em milliss
    if(inicio < pausa) return;


    float esquerda = dist_3;
    float direita = dist_2;
    if (esquerda >= INVALID_DISTANCE) esquerda = 400;      //longe
    if (direita >= INVALID_DISTANCE) direita = 400;

  if (lado == 0) {
    if (esquerda < direita) lado = +1;
    else if (direita < esquerda) lado = -1;
    else lado = (gradiente > 0) ? -1 : +1;

  }

  float frente = dist_1;
  if (frente >= INVALID_DISTANCE) frente = 400;

  float lateral = (lado > 0) ? esquerda : direita;

  if (frente < 15) {
    re = inicio + 700;                      
  }
  else if (frente >= 70 && (lateral < 22 || lateral > 34)) {
    re = inicio + 700;                     
  }
  else {
    vel = 160;
    dir = lado * 200;                  
  }
}


#endif
