#ifndef MOTORES_HPP
#define MOTORES_HPP

/**
 * Tracao e direcao, via duas pontes H de um modulo L298N duplo.
 *
 * Header-only: incluido apenas pelo firmware_unoq.ino. As funcoes sao inline e
 * o estado e static, entao o header pode ser incluido de mais de um lugar sem
 * quebrar a linkagem.
 */

#include <Arduino.h>

/* ─────────── PINOS (datasheet ABX00162, secao 9.6) ───────────
 * Motores nos pinos com ~ (PWM por hardware).
 * D5 e D6 so como digital: ha relato de analogWrite nao gerar PWM neles.
 */
#define PIN_FR   9    // PB8  TIM4_CH3 — tracao frente
#define PIN_TR  10    // PB9  TIM4_CH4 — tracao tras
#define PIN_ESQ  3    // PB0  TIM3_CH3 — direcao esquerda
#define PIN_DIR 11    // PB15 TIM1_CH3N — direcao direita

/* ─────────── PARAMETROS ─────────── */
static const int dutyTracao  = 215;   // 0-255
static const int dutyRe      = 180;   // re mais devagar que a marcha a frente
static const int dutyDirecao = 255; 
  // AJUSTAR NA BANCADA: ver comentario de acionarEsterco

static const unsigned long ESTERCO_MAX_MS    = 2500;  // teto de acionamento continuo
static const unsigned long ESTERCO_ALIVIO_MS = 500;   // descanso obrigatorio apos o teto

// duty > 0 aciona pinoA | duty < 0 aciona pinoB | 0 solta os dois
inline void acionarPonte(int pinoA, int pinoB, int duty) {
  if (duty > 0) {
    analogWrite(pinoB, 0);
    analogWrite(pinoA, duty);
  } else if (duty < 0) {
    analogWrite(pinoA, 0);
    analogWrite(pinoB, -duty);
  } else {
    analogWrite(pinoA, 0);
    analogWrite(pinoB, 0);
  }
}

inline void pararMotores() {
  acionarPonte(PIN_FR,  PIN_TR,  0);
  acionarPonte(PIN_ESQ, PIN_DIR, 0);
}

inline void inicializarMotores() {
  pinMode(PIN_FR,  OUTPUT);   pinMode(PIN_TR,  OUTPUT);
  pinMode(PIN_ESQ, OUTPUT);   pinMode(PIN_DIR, OUTPUT);
  pararMotores();