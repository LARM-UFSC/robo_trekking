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
 * ─────────── DUAS FASES, E O PORQUE ───────────
 *
 * Para ORBITAR um objeto, ele tem que ficar do lado INTERNO da curva. Ao entrar
 * no modo o cone esta A FRENTE -- o robo vinha seguindo ele. Girar de imediato
 * para o lado da orbita joga o cone para o lado de fora da curva, e o robo se
 * afasta em vez de dar a volta.
 *
 * A primeira correcao foi avancar reto ate o cone ficar ao lado. Em pista isso
 * nao funcionou: o robo passava do cone e orbitava o vazio. O que falta e
 * comecar a orbita JA na diagonal, apontando para o lado de fora do cone.
 *
 * Por isso a fase 0: escolhe o lado da orbita (uma vez) e gira por pouco tempo,
 * com pouco avanco, para o sentido OPOSTO ao da orbita. O robo termina a fase
 * apontando na diagonal, com o cone no lado interno do arco que a fase 1 vai
 * descrever. So entao a fase 1 gira para o lado onde o cone esta.
 *
 * O lateral e usado so para ESCOLHER O LADO, uma vez. Nao ha malha fechada de
 * distancia lateral: foi exatamente isso que nao funcionou antes.
 *
 * Se o raio minimo do chassi for maior que a distancia em que se quer orbitar,
 * nenhuma logica resolve -- e geometria. Esse raio nunca foi medido: esterco no
 * batente, uma volta completa, medir o diametro do circulo.
 */

#include <Arduino.h>
#include "motores.hpp"     // dutyRe
#include "ultrassom.hpp"   // dist_1, INVALID_DISTANCE

/* Estrutura herdada da busca, que esta validada. Velocidade agora quase no
 * maximo, na diagonal e na orbita, por decisao de pista; DIR no batente para o
 * circulo ser o menor possivel, que e o que permite orbitar em vez de passar
 * largo. */
static const int VEL_CONTORNO = 240;
static const int DIR_CONTORNO = 255;   // modulo; o sinal vem de contornoLado

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

/* AJUSTAR NA BANCADA: giro inicial para a diagonal, para o sentido OPOSTO ao da
 * orbita. Pouco avanco (VEL_DIAGONAL baixa) e volante no batente. O tempo define
 * o angulo: aumente se ele ainda comeca muito reto, diminua se virar demais e
 * perder o cone. */
static const unsigned long GIRO_DIAGONAL_MS = 3500;
static const int           VEL_DIAGONAL     = 240;   // quase o maximo

/* Trecho reto no fim da diagonal, com o volante centralizado. A orbita gira
 * para o lado OPOSTO ao da diagonal; sem esse trecho o volante vai do batente
 * de um lado direto para o batente do outro, e o robo da um zigue-zague. Com
 * ele, o volante volta ao centro antes de a orbita comecar. Pode ser 0 para
 * desligar. */
static const unsigned long RETO_APOS_DIAGONAL_MS = 300;

static int           contornoFase        = 0;   // 0 = diagonal, 1 = orbita
static unsigned long contornoAvancoAte   = 0;   // fim da fase 0
static bool          contornoAvancoAtivo = false;
static int           contornoLado        = 0;   // +1 esquerda, -1 direita
static bool          contornoRecuando    = false;
static unsigned long contornoRecuandoAte = 0;

/* ═══════════ SERVIDO AO .ino ═══════════ */

// A proxima entrada em contorno comeca girando, nao no meio de uma re.
inline void reiniciarContorno() {
  contornoFase        = 0;
  contornoAvancoAtivo = false;
  contornoLado        = 0;
  contornoRecuando    = false;
}

inline void passoContorno(int gradiente, int &vel, int &dir) {
  const unsigned long agora  = millis();
  const float         frente = (dist_1 < INVALID_DISTANCE) ? dist_1 : 400.0f;

  /* ─── FASE 0: escolhe o lado e vira para a diagonal ─── */
  if (contornoFase == 0) {
    if (!contornoAvancoAtivo) {           // bool, nao sentinela: millis() pode ser 0
      contornoAvancoAtivo = true;
      contornoAvancoAte   = agora + GIRO_DIAGONAL_MS + RETO_APOS_DIAGONAL_MS;

      // Lado escolhido UMA vez, na entrada. Laterais primeiro; empate (ou os
      // dois perdidos) cai no gradiente: positivo = cone a direita = orbitar
      // para a direita = contornoLado -1.
      const float esq  = (dist_3 < INVALID_DISTANCE) ? dist_3 : 400.0f;
      const float dirn = (dist_2 < INVALID_DISTANCE) ? dist_2 : 400.0f;

      if      (esq  < dirn) contornoLado = +1;   // cone a esquerda
      else if (dirn < esq ) contornoLado = -1;   // cone a direita
      else                  contornoLado = (gradiente > 0) ? -1 : +1;
    }

    // Diagonal: volante todo para o lado CONTRARIO ao da orbita, com pouco avanco.
    if (agora < contornoAvancoAte) {
      vel = VEL_DIAGONAL;
      // Nos ultimos RETO_APOS_DIAGONAL_MS o volante centraliza.
      const bool girando = agora < (contornoAvancoAte - RETO_APOS_DIAGONAL_MS);
      dir = girando ? -contornoLado * DIR_CONTORNO : 0;
      return;
    }

    contornoFase = 1;   // diagonal feita: a orbita gira para o lado do cone
  }

  /* ─── FASE 1: orbita ─── */
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
    dir = -contornoLado * DIR_CONTORNO;
  } else {
    vel = VEL_CONTORNO;
    dir =  contornoLado * DIR_CONTORNO;
  }
}

#endif  // CONTORNO_HPP