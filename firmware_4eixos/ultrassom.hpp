/*
 * seis(6) sensores HC-SR04: 3 frontais, 3 traseiros,
 * cobrindo ângulos diagonais entre si.
 * TRIGGER unico compartilhado.
 * Header-only, incluido apenas pelo firmware_unoq.ino.
*/
#ifndef ULTRASSOM_HPP
#define ULTRASSOM_HPP

#include <Arduino.h>

class Ultrassons{
  private:
    int _PIN_TRIGGER;

    int _ECHO_1;      
    int _ECHO_2;      
    int _ECHO_3;      

    int _ECHO_4;      
    int _ECHO_5;
    int _ECHO_6;

    float _INVALID_DISTANCE = 999.0f;
    unsigned long _ECHO_TIMEOUT_US = 25000UL;
    const unsigned long _INTERVALO_SONAR_MS = 60;
    unsigned long _ciclosSonar = 0;
    unsigned long _ultimo = 0; // usado em atualizarSonares

    float _dist_1 = _INVALID_DISTANCE;
    float _dist_2 = _INVALID_DISTANCE;
    float _dist_3 = _INVALID_DISTANCE;
    float _dist_4 = _INVALID_DISTANCE;
    float _dist_5 = _INVALID_DISTANCE;
    float _dist_6 = _INVALID_DISTANCE;


  public:
    Ultrassons(int pino1, int pino2, int pino3, int pino4, int pino5, int pino6, int pino7);
    inline bool houveMedicaoNova(unsigned long &ultimoVisto);
    inline void inicializarSonares();
    inline float usParaCm(unsigned long us);
    inline void atualizarSonares();
    inline float getDist1();
    inline float getDist2();
    inline float getDist3();
    inline float getDist4();
    inline float getDist5();
    inline float getDist6();
};


Ultrassons::Ultrassons(int pino1, int pino2, int pino3, int pino4, int pino5, int pino6, int pino7){
    _ECHO_1 = pino1;      
    _ECHO_2 = pino2;      
    _ECHO_3 = pino3;      

    _ECHO_4 = pino4;      
    _ECHO_5 = pino5;
    _ECHO_6 = pino6;

    _PIN_TRIGGER = pino7;
}


inline bool Ultrassons::houveMedicaoNova(unsigned long &ultimoVisto) {
  if (_ciclosSonar == ultimoVisto) return false;
  ultimoVisto = _ciclosSonar;
  return true;
}

inline void Ultrassons::inicializarSonares() {
  pinMode(_PIN_TRIGGER, OUTPUT);
  digitalWrite(_PIN_TRIGGER, LOW);

  const int echos[] = {_ECHO_1, _ECHO_2, _ECHO_3, _ECHO_4, _ECHO_5, _ECHO_6};
  for (unsigned i = 0; i < sizeof(echos) / sizeof(echos[0]); i++) {
    pinMode(echos[i], INPUT);
  }
}

inline float Ultrassons::usParaCm(unsigned long us) {
  if (us == 0) return _INVALID_DISTANCE; 
  float cm = us / 58.0f;
  if (cm < 4.0f || cm > 400.0f) return _INVALID_DISTANCE;
  return cm;
}

/*
 * Trigger unico: um disparo faz os 6 sensores emitirem juntos, entao os 6
 * ecos precisam ser medidos do MESMO disparo. Fazemos isso varrendo os
 * pinos num laco, registrando subida e descida de cada um.
 * Nao usa pulseIn: so digitalRead e micros().
 */

 
inline void Ultrassons::atualizarSonares() {
  if (millis() - _ultimo < _INTERVALO_SONAR_MS) return;
  _ultimo = millis();

  const int   echos[] = {_ECHO_1, _ECHO_2, _ECHO_3, _ECHO_4, _ECHO_5, _ECHO_6};
  const int   N       = sizeof(echos) / sizeof(echos[0]);
  unsigned long inicio[N], largura[N];
  bool subiu[N], pronto[N];

  for (int i = 0; i < N; i++) {
    inicio[i] = 0; largura[i] = 0;
    subiu[i]  = false; pronto[i] = false;
  }

  // dispara uma unica vez
  
  digitalWrite(_PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(_PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(_PIN_TRIGGER, LOW);

  unsigned long t0 = micros(); 
  int faltam = N; 

  while (faltam > 0 && (micros() - t0) < _ECHO_TIMEOUT_US) { 
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

  _dist_1 = pronto[0] ? usParaCm(largura[0]) : _INVALID_DISTANCE;
  _dist_2 = pronto[1] ? usParaCm(largura[1]) : _INVALID_DISTANCE;
  _dist_3 = pronto[2] ? usParaCm(largura[2]) : _INVALID_DISTANCE;
  _dist_4 = pronto[3] ? usParaCm(largura[3]) : _INVALID_DISTANCE;
  _dist_5 = pronto[4] ? usParaCm(largura[4]) : _INVALID_DISTANCE;
  _dist_6 = pronto[5] ? usParaCm(largura[5]) : _INVALID_DISTANCE;
  _ciclosSonar++;
}


inline float Ultrassons::getDist1(){
  return _dist_1;
}
inline float Ultrassons::getDist2(){
  return _dist_2;
}
inline float Ultrassons::getDist3(){
  return _dist_3;
}
inline float Ultrassons::getDist4(){
  return _dist_4;
}
inline float Ultrassons::getDist5(){
  return _dist_5;
} 
inline float Ultrassons::getDist6(){
  return _dist_6;
}
#endif