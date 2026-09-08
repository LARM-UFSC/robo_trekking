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
static const int dutyRe      = 140;   // re mais devagar que a marcha a frente
static const int dutyDirecao = 190;   // AJUSTAR NA BANCADA: ver comentario de acionarEsterco

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
}

inline int estercoPedido(char cmd) {
  if (cmd == 'L') return  dutyDirecao;
  if (cmd == 'R') return -dutyDirecao;
  return 0;                                 // 'F', 'B' e 'S': esterco solto
}

/*
 * O esterco tem mola de centragem: solto, ele volta ao centro sozinho. Logo a
 * deflexao so se sustenta com o motor acionado, e o duty define o ponto de
 * equilibrio entre o torque do motor e a mola -- o duty E o comando de posicao.
 *
 * Por isso NAO ha mais pulso de 250 ms: aquilo era um PWM de 2 Hz por cima do
 * PWM real e fazia o esterco oscilar entre a deflexao e o centro, em vez de
 * segurar o angulo pedido. Ajuste o angulo por dutyDirecao, nao por tempo: se
 * o esterco encosta no batente, o duty esta alto demais e so gera corrente.
 *
 * Sobra o risco termico: segurar angulo contra a mola e rotor bloqueado, ainda
 * que longe do batente. Controlamos por duty (corrente) e por um teto de tempo
 * ligado, com alivio obrigatorio depois.
 */
inline void acionarEsterco(char cmd) {
  static int  ladoAnterior = 0;
  static unsigned long ligadoDesde = 0;
  static unsigned long alivioAte   = 0;

  const unsigned long agora = millis();
  const int lado = estercoPedido(cmd);

  // trocar de lado tira o motor do batente: o acumulado recomeca
  if (lado != ladoAnterior) {
    ladoAnterior = lado;
    ligadoDesde  = agora;
  }

  if (lado == 0 || agora < alivioAte) {
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
    return;
  }

  if (agora - ligadoDesde >= ESTERCO_MAX_MS) {   // teto atingido: alivia
    alivioAte   = agora + ESTERCO_ALIVIO_MS;
    ligadoDesde = alivioAte;
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
    return;
  }

  acionarPonte(PIN_ESQ, PIN_DIR, lado);
}

inline void aplicarComando(char cmd) {
  if (cmd == 'F' || cmd == 'L' || cmd == 'R') {
    acionarPonte(PIN_FR, PIN_TR, dutyTracao);
  } else if (cmd == 'B') {
    acionarPonte(PIN_FR, PIN_TR, -dutyRe);   // re
  } else {
    acionarPonte(PIN_FR, PIN_TR, 0);         // 'S' e qualquer byte estranho
  }

  acionarEsterco(cmd);
}

#endif  // MOTORES_HPP
