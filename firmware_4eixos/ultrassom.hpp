#ifndef ULTRASSOM_HPP
#define ULTRASSOM_HPP

/**
 * seis(6) sensores HC-SR04: 3 frontais, 3 traseiros,
 * cobrindo ângulos diagonais entre si.
 * TRIGGER unico compartilhado.
 * Header-only, incluido apenas pelo firmware_unoq.ino.
 */

#include <Arduino.h>

#define PIN_TRIGGER  //definir

#define ECHO_1      //definir
#define ECHO_2      //definir
#define ECHO_3      //...

#define ECHO_4      
#define ECHO_5
#define ECHO_6

#define INVALID_DISTANCE 999.0f
#define ECHO_TIMEOUT_US  25000UL   // ~4 m ida e volta

static const unsigned long INTERVALO_SONAR_MS = 60; //como sabe que 60 ta em ms??

/* Incrementa a cada medicao concluida. Quem precisa de debounce compara com o
 * ultimo valor visto: o loop() roda muito mais rapido que INTERVALO_SONAR_MS, e
 * um contador incrementado por iteracao saturaria sobre a MESMA leitura. */

static unsigned long ciclosSonar = 0;

static float dist_1 = INVALID_DISTANCE;
static float dist_2 = INVALID_DISTANCE;
static float dist_3 = INVALID_DISTANCE;
static float dist_4 = INVALID_DISTANCE;
static float dist_5 = INVALID_DISTANCE;
static float dist_6 = INVALID_DISTANCE;


/* true uma unica vez por medicao. Cada chamador guarda o proprio marcador, por
 * isso o parametro -- dois consumidores nao se atropelam. */
inline bool houveMedicaoNova(unsigned long &ultimoVisto) { //como vai saber o ultimo visto?
  if (ciclosSonar == ultimoVisto) return false;
  ultimoVisto = ciclosSonar;
  return true;
}

inline void inicializarSonares() {
  pinMode(PIN_TRIGGER, OUTPUT);
  digitalWrite(PIN_TRIGGER, LOW);

  const int echos[] = {ECHO_1, ECHO_2, ECHO_3, ECHO_4, ECHO_5, ECHO_6};
  for (unsigned i = 0; i < sizeof(echos) / sizeof(echos[0]); i++) { //por que esse /sizeof(echos[0)])?
    pinMode(echos[i], INPUT);
  }
}

inline float usParaCm(unsigned long us) {
  if (us == 0) return INVALID_DISTANCE; 
  float cm = us / 58.0f;
  if (cm < 4.0f || cm > 400.0f) return INVALID_DISTANCE;
  return cm;
}

/*
 * Trigger unico: um disparo faz os 6 sensores emitirem juntos, entao os 6
 * ecos precisam ser medidos do MESMO disparo. Fazemos isso varrendo os
 * pinos num laco, registrando subida e descida de cada um.
 * Nao usa pulseIn: so digitalRead e micros().
 */

 
inline void atualizarSonares() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < INTERVALO_SONAR_MS) return; //pra que essa conferida estranha? do que se trata?
  ultimo = millis();

  const int   echos[] = {ECHO_1, ECHO_2, ECHO_3, ECHO_4, ECHO_5, ECHO_6};
  const int   N       = sizeof(echos) / sizeof(echos[0]); //por que divide pelo echos na posicao 0?
  unsigned long inicio[N], largura[N];
  bool subiu[N], pronto[N];

  for (int i = 0; i < N; i++) { //aqui pelo visto ta preparando as variaveis de teste ne?
    inicio[i] = 0; largura[i] = 0;
    subiu[i]  = false; pronto[i] = false;
  }

  // dispara uma unica vez
  //por que 2 e depois 10 micross?
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  unsigned long t0 = micros(); //esse micros() é o tempo atual do processamento ne?
  int faltam = N; //como assim faltam n? esse n é declarado logo ali em cima pra ser o que?

  while (faltam > 0 && (micros() - t0) < ECHO_TIMEOUT_US) { //traduza isso aí pra linguagem humana
    for (int i = 0; i < N; i++) {
      if (pronto[i]) continue;

      int nivel = digitalRead(echos[i]); //pelo visto ele vai ler cada echo por vez do while

      if (!subiu[i]) {
        if (nivel == HIGH) { subiu[i] = true; inicio[i] = micros(); }
      } else if (nivel == LOW) { // e isso aqui é o calculo de largura de cada echo/sensor ou seja distancia é isso?
        largura[i] = micros() - inicio[i];
        pronto[i]  = true; //pra que esse pronto?
        faltam--;
      }
    }
  }

  dist_1 = pronto[0] ? usParaCm(largura[0]) : INVALID_DISTANCE;
  dist_2 = pronto[1] ? usParaCm(largura[1]) : INVALID_DISTANCE;
  dist_3 = pronto[2] ? usParaCm(largura[2]) : INVALID_DISTANCE;
  dist_4 = pronto[3] ? usParaCm(largura[3]) : INVALID_DISTANCE;
  dist_5 = pronto[4] ? usParaCm(largura[4]) : INVALID_DISTANCE;
  dist_6 = pronto[5] ? usParaCm(largura[5]) : INVALID_DISTANCE;
  ciclosSonar++;
}

#endif