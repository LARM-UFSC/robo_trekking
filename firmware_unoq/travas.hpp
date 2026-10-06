#ifndef TRAVAS_HPP
#define TRAVAS_HPP

/**
 * Travas de seguranca: a ultima palavra sobre o que chega aos motores.
 *
 * Implementado em 2026-10-06: frontal critico e cercado.
 *
 * Roda DEPOIS do modo produzir vel/dir e ANTES do acionamento. E o unico ponto
 * do firmware capaz de zerar os motores, de proposito: quando o robo parar sem
 * motivo aparente, ha um lugar so para olhar.
 *
 * ─────────── O INVARIANTE ───────────
 *
 * AS TRAVAS SO REDUZEM. Elas podem zerar velocidade e soltar o esterco, nunca
 * aumentar duty nem apontar a direcao para outro lado. E esse invariante que
 * permite raciocinar sobre o pior caso sem simular a composicao do fuzzy
 * inteira -- qualquer que seja a saida do controlador, o que passa daqui e
 * igual ou mais conservador.
 *
 * Nenhuma trava depende de MODO. Por isso o enum Modo fica confinado ao .ino:
 * o que e seguranca vale em qualquer comportamento. Parar ao chegar no cone NAO
 * e trava -- e fase do contorno, e mora no contorno.hpp.
 *
 * ─────────── AS TRAVAS SAO DIRECIONAIS ───────────
 *
 * Este e o detalhe que, se passar, quebra a busca. Obstaculo a frente NAO pode
 * bloquear a re: a busca entra em re justamente por causa da parede da frente,
 * e uma trava cega zeraria a velocidade no mesmo ciclo, prendendo o robo contra
 * a parede para sempre.
 *
 *   if (vel > 0 && dist_1 < CRITICO_CM) vel = 0;   // so o avanco
 *
 * Quando o sensor traseiro entrar, vale o espelho com dist_4 e vel < 0.
 *
 * ─────────── DUAS ARMADILHAS ───────────
 *
 * DEBOUNCE POR MEDICAO, nao por iteracao. O loop() roda a ~80 ms e o sonar mede
 * a cada 60 ms, entao um contador incrementado a cada volta satura sobre a MESMA
 * leitura e o filtro nao filtra nada. Use houveMedicaoNova() do ultrassom.hpp,
 * com um marcador proprio para cada trava.
 *
 * HISTERESE. Com limiar unico, leitura oscilando em torno dele faz o robo
 * alternar entre parado e andando varias vezes por segundo. O limiar de soltar
 * tem que ser maior que o de travar.
 *
 * ─────────── ORDENACAO DOS LIMIARES ───────────
 *
 * Os limiares das travas tem que ser MENORES que os das manobras. A manobra age
 * primeiro e tenta se resolver; a trava e a rede embaixo dela. Hoje:
 *
 *   trava critica (15)  <  contorno (25)  <  busca (40)
 */

#include <Arduino.h>
#include "ultrassom.hpp"   // dist_1..3, houveMedicaoNova

/* AJUSTAR: distancia frontal abaixo da qual o avanco e proibido, independente
 * de modo. Tem que ficar abaixo do limiar de qualquer manobra. */
static const float CRITICO_CM = 15.0f;

/* AJUSTAR: cercado -- os tres sensores abaixo deste valor. */
static const float CERCADO_CM = 30.0f;

// Histerese: so volta a andar quando algum lado abrir mais que isto.
static const float CERCADO_LIVRE_CM = 45.0f;

/* Medicoes seguidas para declarar cercado. Nao se aplica a trava critica, que
 * reage no mesmo ciclo -- ver o comentario dela. */
static const int CONFIRMACOES_CERCADO = 2;

/* ═══════════ SERVIDO AO .ino ═══════════ */

/*
 * Aplica as travas sobre a saida do modo. Pode reduzir, nunca aumentar.
 *
 * A trava de link caido nao esta aqui: ela e resolvida no .ino, que simplesmente
 * nao chama modo nenhum quando o comando esta velho.
 */
inline void aplicarTravas(int &vel, int &dir) {

  /* ─── 1. FRONTAL CRITICO ───
   * Ultimo recurso, so contra o avanco. Sem debounce de proposito: e a guarda
   * final contra colisao, e atrasar a reacao em duas medicoes (120 ms) para
   * filtrar ruido custa mais do que o falso positivo, que gasta um ciclo
   * parado e se resolve sozinho no proximo.
   *
   * 999 (eco perdido) nao e menor que o limiar, entao sensor sem eco nao
   * dispara trava -- mesma escolha documentada no ultrassom.hpp. */
  if (vel > 0 && dist_1 < CRITICO_CM) vel = 0;

  /* ─── 2. CERCADO ───
   * Os tres sensores bloqueados: nao ha lado bom e seguir em frente so raspa.
   *
   * Debounce POR MEDICAO, nao por iteracao: o loop() roda muito mais rapido
   * que INTERVALO_SONAR_MS, e um contador por volta saturaria sobre a mesma
   * leitura sem filtrar nada.
   *
   * Histerese assimetrica: tranca com os tres abaixo de CERCADO_CM, solta
   * quando ALGUM passar de CERCADO_LIVRE_CM. Soltar exige apenas uma saida,
   * porque uma saida basta para escapar.
   *
   * Leitura invalida conta como livre, entao sensor falhando impede a trava de
   * trancar. Falha para o lado de seguir andando -- consistente com o resto do
   * firmware, e registrado como escolha, nao descuido. */
  static unsigned long ultimaMedicao = 0;
  static bool cercado  = false;
  static int  confirma = 0;

  if (houveMedicaoNova(ultimaMedicao)) {
    if (cercado) {
      const bool algumAbriu = dist_1 > CERCADO_LIVRE_CM
                           || dist_2 > CERCADO_LIVRE_CM
                           || dist_3 > CERCADO_LIVRE_CM;
      if (algumAbriu) { cercado = false; confirma = 0; }
    }
    else {
      const bool tresBloqueados = dist_1 < CERCADO_CM
                               && dist_2 < CERCADO_CM
                               && dist_3 < CERCADO_CM;
      if (tresBloqueados) {
        if (++confirma >= CONFIRMACOES_CERCADO) cercado = true;
      } else {
        confirma = 0;
      }
    }
  }

  /* So o avanco, igual a trava critica. Bloquear a re tambem deixaria o robo
   * morto no canto: a busca entra em re justamente porque a frente esta
   * fechada, e uma trava cega a direcao anularia a unica saida que existe.
   *
   * A contrapartida e que a fuga e as cegas enquanto o ECHO_4 nao estiver
   * ligado -- limitada pelo teto de tempo da re, em busca.hpp. Quando o
   * traseiro entrar, vale o espelho: if (vel < 0 && dist_4 < CRITICO_CM). */
  if (cercado && vel > 0) vel = 0;

  /* ─── 3. PARADO NAO PRECISA DE ESTERCO ───
   * Com vel em zero o esterco nao desloca nada, e segurar o angulo contra a
   * mola e exatamente o caso que esquenta o motor. Soltar aqui e reducao, nao
   * mudanca de direcao, entao respeita o invariante.
   *
   * Nao vale para vel < 0: a re da busca depende do contra-esterco. */
  if (vel == 0) dir = 0;
}

#endif  // TRAVAS_HPP
