#ifndef CONTORNO_HPP
#define CONTORNO_HPP

/**
 * Contorno do cone: da a volta no cone alcancado, ate a camera ver outro.
 *
 * MESMA ESTRUTURA DA BUSCA (busca.hpp), com limiares proprios. Essa escolha e
 * deliberada: a manobra da busca -- circulo no batente, com re de
 * CONTRA-ESTERCO quando bloqueia -- e a unica coisa deste firmware que foi
 * validada em pista. As tentativas anteriores de controlar a distancia lateral
 * ao cone nao funcionaram, e o motivo e estrutural:
 *
 *   - o acionamento e liga-desliga, entao o robo nao consegue SEGURAR um raio;
 *     ele descreve o raio que a geometria da no batente e nada mais;
 *   - o lateral perde de 6% a 15% das leituras abaixo de 50 cm (medido), e cone
 *     e refletor pior que parede -- pequeno e arredondado;
 *   - sem o inválido tratado, perder o cone de vista virava "longe demais" e
 *     mandava o robo para a re.
 *
 * Entao aqui NAO ha controle de faixa lateral. O robo descreve o menor circulo
 * que consegue e recua quando encontra algo a frente. Quem decide quando parar
 * de contornar e a CPU, no DecidirModo() do ia_trekking.py: o modo sai sozinho
 * quando a camera enxerga um cone longe.
 *
 * Se o raio minimo do chassi for maior que a distancia em que se quer orbitar,
 * nenhuma logica resolve -- e geometria. Esse raio nunca foi medido: esterco no
 * batente, uma volta completa, medir o diametro do circulo.
 */

#include <Arduino.h>
#include "motores.hpp"     // dutyRe
#include "ultrassom.hpp"   // dist_1, INVALID_DISTANCE

/* Herdados da busca, que esta validada. VEL menor que o avanco normal porque a
 * orbita acontece perto das coisas; DIR no batente para o circulo ser o menor
 * possivel, que e o que permite orbitar em vez de passar largo. */
static const int VEL_CONTORNO = 170;
static const int DIR_CONTORNO = 255;   // + = esquerda (PIN_ESQ)

/* Limiares menores que os da busca (40 e 70): o contorno trabalha perto, e o
 * proprio cone e o obstaculo a frente. E precisam ficar ACIMA do CRITICO_CM
 * (15) das travas -- a manobra reage primeiro, a trava e a rede embaixo dela.
 *
 *   trava critica (15)  <  contorno (25)  <  busca (40)
 *
 * Os dois valores diferentes dao histerese: com um limiar unico o robo
 * trepidaria entre avanco e re na fronteira. */
static const float CONTORNO_BLOQUEIO = 25.0f;   // abaixo disto, entra em re
static const float CONTORNO_LIVRE    = 45.0f;   // acima disto, volta a girar

/* A re e CEGA: o sensor traseiro (ECHO_4) nao esta ligado. O teto limita o
 * estrago, nao evita a colisao. Quando o traseiro entrar, a condicao de saida
 * passa a ser ele e o teto vira so rede. */
static const unsigned long TETO_RE_CONTORNO_MS = 900;

static bool          contornoRecuando    = false;
static unsigned long contornoRecuandoAte = 0;

/* ═══════════ SERVIDO AO .ino ═══════════ */

// A proxima entrada em contorno comeca girando, nao no meio de uma re.
inline void reiniciarContorno() {
  contornoRecuando = false;
}

inline void passoContorno(int gradiente, int &vel, int &dir) {
  (void)gradiente;   // lado do giro e fixo; a posicao do cone nao entra aqui

  const unsigned long agora  = millis();
  const float         frente = (dist_1 < INVALID_DISTANCE) ? dist_1 : 400.0f;

  if (contornoRecuando) {
    const bool liberou  = frente > CONTORNO_LIVRE;
    const bool estourou = agora >= contornoRecuandoAte;
    if (liberou || estourou) contornoRecuando = false;
  }
  else if (frente < CONTORNO_BLOQUEIO) {
    contornoRecuando    = true;
    contornoRecuandoAte = agora + TETO_RE_CONTORNO_MS;
  }

  /* Contra-esterco na re: recuar com o MESMO esterco percorre o arco de volta e
   * desfaz a rotacao que o avanco produziu. Com o esterco invertido, os dois
   * segmentos somam rotacao no mesmo sentido -- e a manobra de tres tempos, e e
   * o que faz a orbita progredir em vez de oscilar entre dois pontos. */
  if (contornoRecuando) {
    vel = -dutyRe;
    dir = -DIR_CONTORNO;
  } else {
    vel = VEL_CONTORNO;
    dir = DIR_CONTORNO;
  }
}

#endif  // CONTORNO_HPP
