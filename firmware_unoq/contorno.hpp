#ifndef CONTORNO_HPP
#define CONTORNO_HPP

/**
 * Contorno do cone: da a volta no cone que o robo acabou de alcancar.
 *
 * Serve ao modo CONTORNAR. ESQUELETO -- a implementar.
 *
 * ─────────── O QUE O MODO SIGNIFICA ───────────
 *
 * Quem sequencia os modos e a CPU, no DecidirModo() do ia_trekking.py:
 *
 *   cone visivel e perto   -> "contornar"  (e marca contornando = True)
 *   cone visivel e longe   -> "aproximar"  (e limpa contornando)
 *   sem cone + contornando -> "contornar"
 *   sem cone               -> "procurar"
 *
 * Ou seja: o modo entra ao chegar perto, PERMANECE mesmo depois de perder o cone
 * de vista, e sai sozinho no instante em que a camera enxerga um cone longe --
 * que e exatamente "quando ver outro, segue o outro". Este arquivo nao precisa
 * sequenciar nada nem decidir quando terminar: so executar enquanto durar.
 *
 * O robo NAO para no cone. Ele contorna.
 *
 * ─────────── MECANICA ───────────
 *
 * Mesma manobra da busca: esterco no batente, tracao a frente, e re com
 * CONTRA-ESTERCO quando bloqueia. Com Ackermann no batente o raio e o minimo
 * possivel, que e o que permite orbitar em volta do cone.
 *
 * Ver busca.hpp para o porque do contra-esterco: recuar com o mesmo esterco
 * desfaz a rotacao que o avanco produziu, e o robo nunca progride.
 *
 * Lado fixo, como na busca -- previsibilidade vale mais que otimalidade em
 * prova, porque voce sempre sabe de que lado ele vai sair.
 *
 * ─────────── A DIFERENCA QUE IMPORTA ───────────
 *
 * No contorno, o PROPRIO CONE e o obstaculo a frente. Com o limiar da busca
 * (40 cm) o cone dispararia re a cada volta e a orbita nunca fecharia. O limiar
 * daqui precisa ser menor que a distancia em que o robo passa raspando no cone.
 *
 * E ele ainda precisa ser MAIOR que o da trava de frontal critico, senao a
 * trava zera a velocidade antes da manobra ter chance de agir, e o robo congela
 * encarando o cone.
 *
 *   trava critica  <  limiar do contorno  <  limiar da busca
 */

#include <Arduino.h>
#include "motores.hpp"     // dutyRe
#include "ultrassom.hpp"   // dist_1..3

/* AJUSTAR: menor que LIMIAR_BUSCA_BLOQUEIO, maior que a trava critica. */
static const float LIMIAR_CONTORNO_BLOQUEIO = 25.0f;

/* AJUSTAR NA BANCADA: duty de tracao na orbita. */
static const int VEL_CONTORNO = 150;

// Lado da orbita. +255 e esquerda (PIN_ESQ), -255 e direita.
static const int DIR_CONTORNO = 255;

/* ═══════════ SERVIDO AO .ino ═══════════ */

// A proxima entrada em contorno comeca do zero, nao no meio de uma manobra.
inline void reiniciarContorno() {
  // a implementar
}

/*
 * Um ciclo do contorno. Recebe o gradiente porque a posicao do cone pode ser
 * util para fechar a orbita; se nao for, o parametro sai da assinatura.
 */
inline void passoContorno(int gradiente, int &vel, int &dir) {
  // a implementar -- por ora nao move, para nao fingir comportamento
  vel = 0;
  dir = 0;
}

#endif  // CONTORNO_HPP
