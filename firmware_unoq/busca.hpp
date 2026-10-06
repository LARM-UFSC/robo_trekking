#ifndef BUSCA_HPP
#define BUSCA_HPP

/**
 * Busca do cone: gira em circulo ate a camera encontrar algum.
 *
 * Serve ao modo PROCURAR. O .ino nao precisa saber nada daqui alem das duas
 * funcoes servidas no fim do arquivo.
 *
 * NOTA SOBRE DUPLICACAO: o contorno do cone (contorno.hpp) usa a mesma manobra
 * de circulo com re de contra-esterco, e por ora cada um tem a sua copia. E
 * escolha deliberada, para os dois serem escritos em paralelo sem conflito.
 * Quando os dois estiverem funcionando, vale extrair a primitiva comum --
 * lembrando que, ate isso acontecer, consertar a re e consertar nos DOIS.
 */

#include <Arduino.h>
#include "motores.hpp"     // dutyRe
#include "ultrassom.hpp"   // dist_1..3

/* AJUSTAR NA BANCADA: duty de tracao durante a busca. Suficiente para descrever
 * o circulo sem arrastar, e baixo o bastante para nao passar longe de um cone
 * que entrou no campo de visao. */
static const int VEL_BUSCA = 170;

// Lado do circulo. +255 e esquerda (PIN_ESQ), -255 e direita.
static const int DIR_BUSCA = 255;

/* ─────────── RE COM CONTRA-ESTERCO ───────────
 * O circulo acaba encontrando parede em espaco fechado. Quando isso acontece o
 * robo recua -- mas com o esterco INVERTIDO.
 *
 * Geometria Ackermann: avancar esterçado a esquerda gira o robo no sentido
 * anti-horario; recuar com o MESMO esterco percorre o arco de volta e gira no
 * sentido oposto, desfazendo exatamente o que o avanco fez. O robo oscilaria
 * entre dois pontos varrendo sempre o mesmo pedaco do horizonte, sem nunca
 * completar uma volta.
 *
 * Com o esterco invertido, os dois segmentos somam rotacao no mesmo sentido. E
 * a manobra de tres tempos, e e o que permite girar em espaco apertado.
 *
 * A saida da re e MEDIDA, nao cronometrada: recuando de uma parede, dist_1
 * cresce. Os dois limiares diferentes dao a histerese -- com um valor unico o
 * robo trepidaria na fronteira.
 *
 * O teto de tempo existe porque a RE E CEGA: o sensor traseiro (ECHO_4) nao
 * esta ligado. Ele limita o estrago, nao evita a colisao. Quando o traseiro
 * entrar, a condicao de saida da re passa a ser ele e o teto vira so rede.
 *
 * IMPORTANTE: estes limiares precisam ser MAIORES que os das travas. Se a trava
 * de frontal critico disparar antes da manobra comecar a recuar, ela zera a
 * velocidade e o robo congela encarando a parede, sem chance de se resolver.
 */
static const float LIMIAR_BUSCA_BLOQUEIO = 40.0f;   // abaixo disto, entra em re
static const float LIMIAR_BUSCA_LIVRE    = 70.0f;   // acima disto, volta a avancar
static const unsigned long TETO_RE_BUSCA_MS = 1500;

static bool          buscaRecuando    = false;
static unsigned long buscaRecuandoAte = 0;

/* Bloqueio para o circulo: a frente, ou o lateral do lado PARA ONDE ele gira.
 * Girando a esquerda, parede a esquerda impede a curva mesmo com a frente
 * livre. 999 (eco perdido) conta como livre, como em todo o resto. */
inline bool buscaBloqueada() {
  const float lateralInterno = (DIR_BUSCA > 0) ? dist_3 : dist_2;  // + = esquerda
  return dist_1 < LIMIAR_BUSCA_BLOQUEIO
      || lateralInterno < LIMIAR_BUSCA_BLOQUEIO;
}

/* ═══════════ SERVIDO AO .ino ═══════════ */

// A proxima entrada em busca comeca avancando, nao no meio de uma re.
inline void reiniciarBusca() {
  buscaRecuando = false;
}

inline void passoBusca(int &vel, int &dir) {
  const unsigned long agora = millis();

  if (buscaRecuando) {
    const bool liberou  = dist_1 > LIMIAR_BUSCA_LIVRE;
    const bool estourou = agora >= buscaRecuandoAte;
    if (liberou || estourou) buscaRecuando = false;
  }
  else if (buscaBloqueada()) {
    buscaRecuando    = true;
    buscaRecuandoAte = agora + TETO_RE_BUSCA_MS;
  }

  if (buscaRecuando) {
    vel = -dutyRe;        // re
    dir = -DIR_BUSCA;     // contra-esterco: mantem o sentido do giro
  } else {
    vel = VEL_BUSCA;
    dir = DIR_BUSCA;
  }
}

#endif  // BUSCA_HPP
