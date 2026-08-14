#ifndef ULTRASSOM_NODE_HPP
#define ULTRASSOM_NODE_HPP

#include <NewPing.h>

#define LED_AZUL 2

extern volatile char comandoCamera; 

// ULTRASSON - CONFIGURAÇÃO NEWPING
extern const int PIN_TRIGGER;   
extern const int PIN_ECHO_1; 
extern const int PIN_ECHO_2;  
extern const int PIN_ECHO_3;
extern const int PIN_ECHO_4;

extern NewPing sonar1;
extern NewPing sonar2;
extern NewPing sonar3;
extern NewPing sonar4;

extern const long intervaloCicloTotal; 

extern volatile float dist_1, dist_2, dist_3, dist_4;

float processarLeituraNewPing(unsigned int ping_us);

void TaskSensores(void * pvParameters);

void SinalVidaSensores(void);



#endif