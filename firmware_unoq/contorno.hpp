#ifndef CONTORNO_HPP
#define CONTORNO_HPP

#include "ultrassom.hpp"
// contorna o cone assim que entra no modo contornar pelo ia_trekking.py
// utilizando os sensores ultrassom. contorna ate ver outro cone longe e aproxiomar novamente

unsigned long inicio = 0;
unsigned long inicioRe = 0;
int dirRe = 0;


void reiniciarContorno(){
  inicio = 0;
  inicioRe = 0;
}

void passoContorno(int gradiente, int &vel, int &dir){
  //tempo = tempo atual de funcionamento(nao importa qual seja)
  //enquanto a diferença entre o tempo atual e o tempo de entrada na função for menor que 1 segundo,
  //o robô fica numa pausa de debug experimental.
  if(inicio==0) inicio = millis();

  unsigned long tempoExecucao = millis() - inicio;

  //dois segundos de pausa debug
  if(tempoExecucao < 2000){
    dir = 0;
    vel = 0;
  } else if(tempoExecucao < 4000){
    dir = 250;
    vel = 220;
  } else if(tempoExecucao < 5000){
    dir = -220;
    vel = 180;
  } else if(tempoExecucao < 6000){
    dir = 0;
    vel = 70;
  } else{
    if(inicioRe != 0){
      if (millis() - inicioRe < 400){
        vel = -120;
        dir = dirRe;
      } else{ //acabou a re
        inicioRe = 0;
      }
    } else if (dist_2 < 15) { //confere se ta perto demais
      inicioRe = millis();
      dirRe = 200;
    } else if (dist_2 > 70) { //longe demais
      inicioRe = millis();
      dirRe = 200;
    } else { //faixa certa, gira
      vel = 220;
      dir = 200;
    }

  }

  //depois:
  //while ultra esquerda nao tiver na faixa aceitavel fica manobrando com ré
  //se ultra esquerda tiver na faixa aceitavel, só dir = -tanto e vel pra ele girar.
  //assim que esquerda sair dessa faixa tanto mt perto quanto mt longe ele volta a manobrar pra ficar na faixa
  //basicamente se esquerda entre tal e tal, gira. se dist esq maior, re e dir -. se dist esq menor, re e dir+
}


#endif
