#ifndef ULTRASSOM_HPP
#define ULTRASSOM_HPP

/**
 * 3 sensores HC-SR04 na frente, com TRIGGER unico compartilhado.
 * O 4o (traseiro) esta reservado, ainda nao ligado.
 *
 * Header-only, ativado por USAR_ULTRASSOM. A flag e definida no
 * firmware_unoq.ino ANTES deste include -- com ela em 0, este arquivo nao
 * gera codigo nenhum.
 */

#include <Arduino.h>

#ifndef USAR_ULTRASSOM
  #error "Defina USAR_ULTRASSOM antes de incluir ultrassom.hpp"
#endif

#if USAR_ULTRASSOM

// 3 sensores na frente, 1 atras. TRIGGER unico, comum a todos.
#define PIN_TRIGGER 2    // PB3   disparo compartilhado
#define ECHO_1      4    // PA12  frente
#define ECHO_2      6    // PB1   frente
#define ECHO_3      8    // PB4   frente
// Sensor 4 (traseiro) desativado por enquanto. Pino reservado.
// #define ECHO_4  13    // PB13  traseiro
// Livres apos unificar o trigger: D5, D7, D12

#define INVALID_DISTANCE 999.0f
#define ECHO_TIMEOUT_US  25000UL   // ~4 m ida e volta

static const unsigned long INTERVALO_SONAR_MS = 60;

static float dist_1 = INVALID_DISTANCE, dist_2 = INVALID_DISTANCE;
static float dist_3 = INVALID_DISTANCE;
// static float dist_4 = INVALID_DISTANCE;   // traseiro

inline void inicializarSonares() {
  pinMode(PIN_TRIGGER, OUTPUT);
  digitalWrite(PIN_TRIGGER, LOW);

  const int echos[] = {ECHO_1, ECHO_2, ECHO_3 /*, ECHO_4 */};
  for (unsigned i = 0; i < sizeof(echos) / sizeof(echos[0]); i++) {
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
 * Trigger unico: um disparo faz os 3 sensores emitirem juntos, entao os 3
 * ecos precisam ser medidos do MESMO disparo. Fazemos isso varrendo os
 * pinos num laco, registrando subida e descida de cada um.
 * Nao usa pulseIn: so digitalRead e micros().
 */
inline void atualizarSonares() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < INTERVALO_SONAR_MS) return;
  ultimo = millis();

  const int   echos[] = {ECHO_1, ECHO_2, ECHO_3 /*, ECHO_4 */};
  const int   N       = sizeof(echos) / sizeof(echos[0]);
  unsigned long inicio[N], largura[N];
  bool subiu[N], pronto[N];

  for (int i = 0; i < N; i++) {
    inicio[i] = 0; largura[i] = 0;
    subiu[i]  = false; pronto[i] = false;
  }

  // dispara uma unica vez
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  unsigned long t0 = micros();
  int faltam = N;

  while (faltam > 0 && (micros() - t0) < ECHO_TIMEOUT_US) {
    for (int i = 0; i < N; i++) {
      if (pronto[i]) continue;
      int nivel = digitalRead(echos[i]);

      if (!subiu[i]) {
        if (nivel == HIGH) { subiu[i] = true; inicio[i] = micros(); }
      } else if (nivel == LOW) {
        largura[i] = micros() - inicio[i];
        pronto[i]  = true;
        faltam--;
      }
    }
  }

  dist_1 = pronto[0] ? usParaCm(largura[0]) : INVALID_DISTANCE;
  dist_2 = pronto[1] ? usParaCm(largura[1]) : INVALID_DISTANCE;
  dist_3 = pronto[2] ? usParaCm(largura[2]) : INVALID_DISTANCE;
// dist_4 = pronto[3] ? usParaCm(largura[3]) : INVALID_DISTANCE;
}

#endif  // USAR_ULTRASSOM
#endif  // ULTRASSOM_HPP
