/**
 * Firmware do MCU do Arduino Uno Q (STM32U585 / Zephyr).
 *
 * Recebe da CPU Linux, pela Bridge, a posicao do cone (gradiente) e o modo de
 * operacao, e aciona os dois motores: tracao e direcao.
 *
 * Este arquivo nao implementa comportamento. Ele faz tres coisas:
 *   1. recebe o comando da Bridge e guarda o estado
 *   2. traduz esse estado em um modo
 *   3. despacha o modo para o servico correspondente, aplica as travas e aciona
 *
 * Cada comportamento vive no seu proprio header e serve uma funcao com a mesma
 * forma -- entradas por parametro, vel e dir por referencia:
 *
 *   motores.hpp     acionarPonte, aplicarEsterco, pararMotores
 *   ultrassom.hpp   atualizarSonares, dist_1..3, houveMedicaoNova
 *   desvioFuzzy.hpp passoFuzzy       (modo 'a', aproximar)
 *   busca.hpp       passoBusca       (modo 'p', procurar)
 *   contorno.hpp    passoContorno    (modo 'c', contornar)  -- esqueleto
 *   travas.hpp      aplicarTravas                           -- esqueleto
 *   desenharLed.hpp atualizarLedModo
 *   mpu.hpp         lerMPU   (gated por USAR_MPU)
 *
 * Nenhum servico sabe em que modo foi chamado -- ele so sabe executar o proprio
 * comportamento. O despacho por modo existe so neste arquivo.
 *
 * USAR_MPU e a unica flag de compilacao que resta, e precisa ser definida ANTES
 * dos includes: o mpu.hpp depende dela para decidir se gera codigo.
 */

#define USAR_MPU       0   // desligada por enquanto

#include <Arduino_RouterBridge.h>

#include "motores.hpp"
#include "ultrassom.hpp"
#include "desvioFuzzy.hpp"
#include "busca.hpp"
#include "contorno.hpp"
#include "travas.hpp"
#include "mpu.hpp"
#include "desenharLed.hpp"

/* ─────────── PARAMETROS ─────────── */

/*
 * Failsafe: tempo maximo sem comando novo antes de assumir que o link caiu.
 *
 * AJUSTAR: use 3-5x o periodo entre comandos. O FPS sai no log do ia_trekking a
 * cada 30 frames. Medido em 2026-09-25: ~5 FPS, periodo de 200 ms, e os 700 ms
 * abaixo dao 3,5x -- dentro da faixa, mas na ponta apertada. Se a visao ficar
 * mais lenta que 4 FPS, o failsafe passa a disparar em regime normal e o robo
 * anda aos trancos.
 */
const unsigned long TIMEOUT_COMANDO_MS = 700;

/* ─────────── ESTADO ─────────── */
volatile unsigned long ultimoComandoMs = 0;
volatile bool recebeuComando = false;   // separa "nunca recebeu" de "recebeu ha 0 ms"

/* Protocolo da camera: gradiente (-255 a +255, POSITIVO = cone a DIREITA) e modo
 * ("aproximar" / "contornar" / "procurar"). Guardamos so a inicial do modo:
 * String em estado compartilhado com a thread da Bridge e pedir problema. */
volatile int  gradienteCone = 0;
volatile char modoCamera    = '?';   // 'a', 'c', 'p'

/* ═══════════ BRIDGE (CPU Linux -> MCU) ═══════════ */

/* Protocolo de dois argumentos, diferente do bridgemcu.ino original (que
 * recebia uma String com uma letra). Roda em contexto proprio (thread "bridge"
 * do Zephyr), por isso o estado compartilhado e volatile. */
void processa_direcao(int gradiente, String modo) {
  gradienteCone = constrain(gradiente, -255, 255);
  modoCamera    = (modo.length() > 0) ? modo.charAt(0) : '?';

  ultimoComandoMs = millis();
  recebeuComando  = true;
}

// Comando so vale enquanto for recente. Sem isto o robo segue o ultimo
// gradiente para sempre depois que o link cai.
bool comandoEstaFresco() {
  return recebeuComando && (millis() - ultimoComandoMs) < TIMEOUT_COMANDO_MS;
}

/* ═══════════ MODOS ═══════════ */

/*
 * O modo e a inicial do que a CPU manda: 'p' procurar, 'a' aproximar, 'c'
 * contornar. Link caido vira 'x', mesma convencao que o desenharLed.hpp ja usa.
 *
 * Quem sequencia os modos e a CPU, no DecidirModo() do ia_trekking.py -- o MCU
 * nao inventa modo, so executa o que recebe usando os sensores locais.
 *
 * Nao vale criar um enum aqui: o .ino gera prototipos automaticos ANTES das
 * definicoes do arquivo, e funcao que devolve tipo declarado no proprio .ino nao
 * compila. O char resolve sem precisar de header so para o vocabulario.
 */
char modoAtual() {
  if (!comandoEstaFresco()) return 'x';

  switch (modoCamera) {
    case 'p':
    case 'a':
    case 'c': return modoCamera;
    default:  return 'x';   // '?' inicial e qualquer byte estranho
  }
}

/* ═══════════ SETUP / LOOP ═══════════ */

void setup() {
  Serial.begin(115200);

  inicializarMotores();     // configura os pinos e ja deixa tudo parado

  Bridge.begin();
  Bridge.provide("processa_direcao", processa_direcao);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  inicializarSonares();
  inicializarFuzzy();       // monta a base de regras UMA vez: a eFLL aloca com new

#if USAR_MPU
  inicializarMPU();
#endif
  inicializarLedModo();
}

void loop() {
  atualizarSonares();       // dado fresco antes de qualquer decisao

  const char modo = modoAtual();

  int vel = 0, dir = 0;

  switch (modo) {
    case 'p':   // procurar: gira em circulo ate a camera achar um cone
      reiniciarContorno();
      passoBusca(vel, dir);
      break;

    case 'a':   // aproximar: vai atras do cone, desviando de obstaculos
      reiniciarBusca();
      reiniciarContorno();
      passoFuzzy(gradienteCone, vel, dir);
      break;

    case 'c':   // contornar: da a volta no cone alcancado
      reiniciarBusca();
      passoContorno(gradienteCone, vel, dir);
      break;

    default:    /* 'x' -- sem link. Chave geral e botao de emergencia do teste:
                 * matar o ia_trekking para o robo em menos de um segundo.
                 * vel e dir ficam em zero. */
      reiniciarBusca();
      reiniciarContorno();
      break;
  }

  aplicarTravas(vel, dir);  // ultima palavra: so reduz, nunca aumenta

  acionarPonte(PIN_FR, PIN_TR, vel);
  aplicarEsterco(dir);

  // LED aceso = recebendo comando. Diagnostico de bancada sem serial.
  digitalWrite(LED_BUILTIN, modo != 'x' ? HIGH : LOW);

#if USAR_MPU
  lerMPU();
#endif
  atualizarLedModo(modo != 'x', modoCamera);
}
