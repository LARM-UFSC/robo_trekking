#ifndef DESVIO_HPP
#define DESVIO_HPP

/**
 * Desvio de obstaculo pelos ultrassonicos, sobrepondo o comando da camera.
 *
 * Mapeamento fisico (definido na bancada): S1 frente, S2 direita, S3 esquerda.
 *
 * Esterco Ackermann nao gira no proprio eixo: parar de frente para o obstaculo
 * e estercar nao tira o robo do lugar. Por isso o desvio mantem a tracao e vira
 * para o lado mais livre, e o gatilho e longe o bastante para a curva caber.
 * So para de fato quando os tres sensores estao bloqueados.
 *
 * Header-only, gated por USAR_ULTRASSOM. Com a flag em 0 vira passagem direta,
 * entao o .ino nao precisa de #if em volta da chamada.
 */

#include <Arduino.h>

#ifndef USAR_ULTRASSOM
  #error "Defina USAR_ULTRASSOM antes de incluir desvio.hpp"
#endif

// Dependencia explicita: precisamos de dist_1..3. O include guard de
// ultrassom.hpp torna isto inofensivo se o .ino ja tiver incluido antes.
#include "ultrassom.hpp"

#if !USAR_ULTRASSOM

// Ultrassom desligado: o comando da camera passa direto.
inline char aplicarDesvio(char cmdCamera) { return cmdCamera; }
inline bool emDesvio()                    { return false; }

#else

/* AJUSTAR NA BANCADA: meca a distancia que o robo percorre desde o comando ate
 * completar a curva, na velocidade real (dutyTracao). Este valor tem que ser
 * maior que isso, senao o robo encosta no obstaculo no meio da curva. */
static const float DIST_DESVIO_CM = 50.0f;

/* Os 3 sonares compartilham o TRIGGER e disparam juntos, entao o burst de um
 * pode chegar ao receptor do outro (crosstalk) e gerar leitura curta falsa.
 * Exigir leituras seguidas filtra o evento isolado. */
static const int CONFIRMACOES = 2;

static bool desvioLigado = false;

static inline bool bloqueado(float d) {
  /* INVALID_DISTANCE (999) e eco perdido, nao obstaculo -- o HC-SR04 perde eco
   * em superficie angulada o tempo todo, e tratar isso como bloqueio pararia o
   * robo a toa. A consequencia e que sensor com defeito falha para o lado de
   * SEGUIR ANDANDO. E escolha consciente, nao descuido. */
  return d < DIST_DESVIO_CM;
}

inline char aplicarDesvio(char cmdCamera) {
  static int  confirma = 0;
  static char ladoFixo = 0;   // lado escolhido, mantido ate a frente liberar

  if (bloqueado(dist_1)) { if (confirma < CONFIRMACOES) confirma++; }
  else                   { confirma = 0; }

  if (confirma < CONFIRMACOES) {       // frente livre: camera manda
    ladoFixo     = 0;
    desvioLigado = false;
    return cmdCamera;
  }

  const bool dirLivre = !bloqueado(dist_2);
  const bool esqLivre = !bloqueado(dist_3);

  if (!dirLivre && !esqLivre) {        // cercado: para
    ladoFixo     = 0;
    desvioLigado = true;
    return 'S';
  }

  // Escolhe o lado uma unica vez e mantem: reavaliar a cada ciclo faria o robo
  // ziguezaguear quando as duas distancias ficam parecidas.
  if (ladoFixo == 0) ladoFixo = (dist_2 > dist_3) ? 'R' : 'L';

  // ...mas se o lado escolhido fechar e o outro estiver livre, troca.
  if (ladoFixo == 'R' && !dirLivre && esqLivre) ladoFixo = 'L';
  if (ladoFixo == 'L' && !esqLivre && dirLivre) ladoFixo = 'R';

  desvioLigado = true;
  return ladoFixo;
}

inline bool emDesvio() { return desvioLigado; }

#endif  // USAR_ULTRASSOM
#endif  // DESVIO_HPP
