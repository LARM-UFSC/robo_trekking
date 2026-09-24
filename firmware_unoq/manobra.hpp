#ifndef MANOBRA_HPP
#define MANOBRA_HPP

/**
 * Retorno em U: meia volta apos chegar no cone.
 *
 * O robo tem dois motores -- tracao (frente/re) e direcao -- e nenhum encoder.
 * Entao a meia volta e feita com o esterco no batente e tracao a frente, que e
 * exatamente o que o comando 'L' (ou 'R') ja faz em aplicarComando(). A manobra
 * nao precisa de acionamento proprio: ela so segura esse comando pelo tempo
 * necessario.
 *
 * PARADA POR TEMPO, POR ENQUANTO. A condicao de termino esta isolada em
 * retornoUCompleto() de proposito: para migrar ao giroscopio, troque o corpo
 * dessa funcao por "yaw acumulado >= 180 graus" e nada mais no arquivo muda.
 */

#include <Arduino.h>

/* AJUSTAR NA BANCADA -- procedimento:
 *   1) robo no chao, espaco livre em volta, bateria carregada como na prova;
 *   2) forca o comando 'L' continuo e cronometra UMA volta completa (360);
 *   3) TEMPO_U_MS = metade desse tempo;
 *   4) na mesma volta, mede o diametro do circulo descrito. Esse e o espaco
 *      lateral que a manobra exige -- se a pista for mais estreita, o U simples
 *      nao cabe e vai precisar de manobra em tres tempos.
 *
 * O valor abaixo e so um ponto de partida para o primeiro teste. */
static const unsigned long TEMPO_U_MS = 3000;

// Lado para onde o U e feito. 'L' ou 'R'.
static const char LADO_U = 'L';

static bool          uAtivo  = false;
static unsigned long uInicio = 0;

inline void iniciarRetornoU()  { uAtivo = true; uInicio = millis(); }
inline void cancelarRetornoU() { uAtivo = false; }
inline bool retornoUAtivo()    { return uAtivo; }

// ESTA e a funcao a trocar quando o giroscopio entrar.
inline bool retornoUCompleto() {
  return (millis() - uInicio) >= TEMPO_U_MS;
}

/* Devolve o comando do ciclo, ou 0 quando a manobra acabou. */
inline char passoRetornoU() {
  if (!uAtivo) return 0;
  if (retornoUCompleto()) { uAtivo = false; return 0; }
  return LADO_U;
}

#endif  // MANOBRA_HPP
