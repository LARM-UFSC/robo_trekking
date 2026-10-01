#pragma once
#include <Arduino_LED_Matrix.h>

//desenha nos leds do unoQ o modo atual de funcionamento
//nao lea bridge, recebe como parametro do proprio .ino

static ArduinoLEDMatrix matrizModo;

// '#' vai ser definido como aceso
static const char* const APROXIMAR[8] = {
  "......#......",
  ".....###.....",
  "....#####....",
  ".....#.#.....",
  ".....#.#.....",
  ".....#.#.....",
  ".....#.#.....",
  ".....#.#.....",
};
static const char* const CONTORNAR[8] = { 
  "....######...",
  "..##....#####",
  ".##...#..###.",
  ".##..###..#..",
  ".##.#####....",
  ".##.......##.",
  "..##.....##..",
  "...######....",
};
static const char* const PROCURAR[8] = {
  "....###......",
  "...#...#.....",
  "...#...#.....",
  "...#...#.....",
  "....###......",
  ".......#.....",
  "........#....",
  ".........#...",
};
static const char* const NAO_DESENHAR[8] = {
  ".............",
  ".............",
  ".....#.#.....",
  "......#......",
  ".....#.#.....",
  ".............",
  ".............",
  ".............",
};

static void desenharMatriz(const char* const* d) {
  uint8_t frame[8][13];
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 13; c++)
      frame[r][c] = (d[r][c] == '#') ? 1 : 0;
  matrizModo.renderBitmap(frame, 8, 13);
}

inline void inicializarLedModo() {
  matrizModo.begin();
  desenharMatriz(NAO_DESENHAR);
}

inline void atualizarLedModo(bool fresco, char modo) {
  static char ultimo = 0;   // 0 = nada desenhado ainda

  const char m = fresco ? modo : 'x';
  if (m == ultimo) return;
  ultimo = m;

  switch (m) {
    case 'a': desenharMatriz(APROXIMAR); break;
    case 'c': desenharMatriz(CONTORNAR); break;
    case 'p': desenharMatriz(PROCURAR);  break;
    default:  desenharMatriz(NAO_DESENHAR);  break;
  }
}