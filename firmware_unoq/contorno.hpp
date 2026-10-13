#ifndef CONTORNO_HPP
#define CONTORNO_HPP

#include "ultrassom.hpp"
// contorna o cone assim que entra no modo contornar pelo ia_trekking.py
// utilizando os sensores ultrassom. contorna ate ver outro cone longe e aproxiomar novamente
//
// O giro e com dir POSITIVO (= esquerda, PIN_ESQ), entao o cone orbitado fica a
// ESQUERDA e o sensor que o acompanha e o dist_3. Era dist_2 (direita), que
// aponta para o lado vazio: lia longe, caia no ramo "longe demais" e o robo
// recuava quase o tempo todo. Se um dia o giro inverter para dir negativo, o
// sensor tem que inverter junto.

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
    } else if (dist_1 < 25) { //frente fechada
      /* Sem este ramo o contorno TRAVA: a trava critica zera o avanco a 15 cm,
       * a trava solta o esterco porque vel == 0, e no ciclo seguinte o contorno
       * pede avanco de novo -- robo parado para sempre encarando o obstaculo.
       * A trava nao bloqueia re, entao pedir re aqui e a saida.
       *
       * Os 25 cm ficam ACIMA do CRITICO_CM (15) das travas de proposito: a
       * manobra reage primeiro, a trava e a rede embaixo dela. */
      inicioRe = millis();
      dirRe = 0;              //recua reto, so para se afastar da frente
    } else if (dist_3 < 15) { //confere se ta perto demais
      inicioRe = millis();
      dirRe = 200;
    } else if (dist_3 > 70) { //longe demais
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
