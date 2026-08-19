#ifndef MOTORES_NODE_HPP
#define MOTORES_NODE_HPP

//MOTORES
#define PIN_ESQ 23
#define PIN_DIR 4
#define PIN_FR  13
#define PIN_TR  32

//MOTORES CANAL PWN 
#define CH_ESQ  0
#define CH_DIR  1  
#define CH_FR   2
#define CH_TR   3

extern volatile char comandoCamera;

extern const int dutyTracao;   // ~70% — velocidade de avanço
extern const int dutyDirecao;   // força do esterço; baixe se der zigue-zague

extern const unsigned long TEMPO_ESTERCO_MS;   // pulso do esterço

extern const int freq;      
extern const int resolution;   
extern const int dutyCycle;

void inicializarMotores(void);

void acionarPonte(int canalA, int canalB, int duty);

void pararMotores();

void aplicarComando(char cmd);





#endif