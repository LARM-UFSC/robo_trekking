/*
 * Tracao e direcao, via duas pontes H de um modulo L298N duplo.
 * ─────────── PINOS (datasheet ABX00162, secao 9.6) ───────────
 * Motores nos pinos com ~ (PWM por hardware).
 * D5 e D6 so como digital: ha relato de analogWrite nao gerar PWM neles.
*/

#ifndef MOTORES_HPP
#define MOTORES_HPP

#include <Arduino.h>

class Motores{
  private:
    //frente
    int _frenteDireitaFrente;
    int _frenteDireitaTras;

    int _frenteEsquerdaFrente;
    int _frenteEsquerdaTras;

    //tras
    int _trasDireitaFrente;
    int _trasDireitaTras;

    int _trasEsquerdaFrente;
    int _trasEsquerdaTras;

  public:
    Motores(int pino1, int pino2, int pino3, int pino4, 
            int pino5, int pino6, int pino7, int pino8);

    inline void inicializarMotores();
    inline void acionarPonte(int pinoA, int pinoB, int duty);
    inline void pararMotores();

};

Motores::Motores(int pino1, int pino2, int pino3, int pino4, int pino5, int pino6, int pino7, int pino8){
      //frente
    _frenteDireitaFrente = pino1;
    _frenteDireitaTras = pino2;

    _frenteEsquerdaFrente = pino3;
    _frenteEsquerdaTras = pino4;

    //tras
    _trasDireitaFrente = pino5;
    _trasDireitaTras = pino6;

    _trasEsquerdaFrente = pino7;
    _trasEsquerdaTras = pino8;
}


inline void Motores::inicializarMotores() {
  pinMode(_frenteDireitaFrente, OUTPUT);
  pinMode(_frenteDireitaTras, OUTPUT);

  pinMode(_frenteEsquerdaFrente, OUTPUT);
  pinMode(_frenteEsquerdaTras, OUTPUT);

  pinMode(_trasDireitaFrente, OUTPUT);
  pinMode(_trasDireitaTras, OUTPUT);

  pinMode(_trasEsquerdaFrente, OUTPUT);
  pinMode(_trasEsquerdaTras, OUTPUT);
  pararMotores();
}

inline void Motores::acionarPonte(int pinoA, int pinoB, int duty) {
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

inline void Motores::pararMotores() {
  acionarPonte(_frenteDireitaFrente,  _frenteDireitaTras,  0);
  acionarPonte(_frenteEsquerdaFrente, _frenteEsquerdaTras, 0);

  acionarPonte(_trasDireitaFrente,  _trasDireitaTras,  0);
  acionarPonte(_trasEsquerdaFrente, _trasEsquerdaTras, 0);
}

#endif