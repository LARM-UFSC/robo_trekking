#ifndef MOTORES_HPP
#define MOTORES_HPP

/**
 * Tracao e direcao, via duas pontes H de um modulo L298N duplo.
**/

#include <Arduino.h>

/* ─────────── PINOS (datasheet ABX00162, secao 9.6) ───────────
 * Motores nos pinos com ~ (PWM por hardware).
 * D5 e D6 so como digital: ha relato de analogWrite nao gerar PWM neles.
 */


//frente
#define PIN_FR_DIR_FR   //definir
#define PIN_FR_DIR_TS   //definir

#define PIN_FR_ESQ_FR   //...
#define PIN_FR_ESQ_TS

//tras
#define PIN_TR_DIR_FR
#define PIN_TR_DIR_TS

#define PIN_TR_ESQ_FR
#define PIN_TR_ESQ_TS


inline void inicializarMotores() {
  piMode(PIN_FR_DIR_FR, OUTPUT);
  piMode(PIN_FR_DIR_TS, OUTPUT);

  piMode(PIN_FR_ESQ_FR, OUTPUT);
  piMode(PIN_FR_ESQ_TS, OUTPUT);

  piMode(PIN_TR_DIR_FR, OUTPUT);
  piMode(PIN_TR_DIR_TS, OUTPUT);

  piMode(PIN_TR_ESQ_FR, OUTPUT);
  piMode(PIN_TR_ESQ_TS, OUTPUT);
  pararMotores();
}

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
  acionarPonte(PIN_FR_DIR_FR,  PIN_FR_DIR_TS,  0);
  acionarPonte(PIN_FR_ESQ_FR, PIN_FR_ESQ_TS, 0);

  acionarPonte(PIN_TR_DIR_FR,  PIN_TR_DIR_TS,  0);
  acionarPonte(PIN_TR_ESQ_FR, PIN_TR_ESQ_TS, 0);
}

#endif