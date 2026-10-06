#ifndef TRAVAS_HPP
#define TRAVAS_HPP

/**
 * Travas de seguranca: a ultima palavra sobre o que chega aos motores.
 *
 * ESQUELETO -- a implementar.
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

// Histerese: so volta a andar quando a folga passar disto.
static const float CERCADO_LIVRE_CM = 45.0f;

/* ═══════════ SERVIDO AO .ino ═══════════ */

/*
 * Aplica as travas sobre a saida do modo. Pode reduzir, nunca aumentar.
 *
 * A implementar:
 *   1. frontal critico -- dist_1 < CRITICO_CM, SO quando vel > 0
 *   2. cercado         -- os tres abaixo de CERCADO_CM, com histerese e
 *                         debounce por medicao
 *
 * A trava de link caido nao esta aqui: ela e resolvida no .ino, que ja nao
 * chama modo nenhum quando o comando esta velho.
 */
inline void aplicarTravas(int &vel, int &dir) {
  // a implementar -- hoje nao altera nada, igual ao comportamento atual
  (void)vel;
  (void)dir;
}

#endif  // TRAVAS_HPP
