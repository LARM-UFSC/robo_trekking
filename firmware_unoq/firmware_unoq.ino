/**
 * Firmware do MCU do Arduino Uno Q (STM32U585 / Zephyr).
 *
 * Recebe da CPU Linux, pela Bridge, a posicao do cone (gradiente) e o modo de
 * operacao, e aciona os dois motores: tracao e direcao.
 *
 * Organizacao (headers-only, todos incluidos so por este arquivo):
 *   motores.hpp     pinos, pontes H, tracao e esterco
 *   ultrassom.hpp   3 HC-SR04 com trigger unico
 *   desvioFuzzy.hpp base de regras fuzzy do desvio e do seguimento do cone
 *   manobra.hpp     (vazio: a reescrever)
 *   mpu.hpp         MPU6050 via I2C   (gated por USAR_MPU)
 *   desenharLed.hpp indicacao do modo no LED
 *
 * USAR_MPU e a unica flag de compilacao que resta, e precisa ser definida ANTES
 * dos includes: o mpu.hpp depende dela para decidir se gera codigo.
 */

#define USAR_MPU       0   // desligada por enquanto

#include <Arduino_RouterBridge.h>

#include "motores.hpp"
#include "ultrassom.hpp"
#include "desvioFuzzy.hpp"
#include "manobra.hpp"
#include "mpu.hpp"
#include "desenharLed.hpp"

/* ─────────── PARAMETROS ─────────── */
const unsigned long INTERVALO_LOG_MS = 300;

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

// Periodo de envio das distancias ao lado Linux. Nao adianta ser menor que o
// INTERVALO_SONAR_MS (60 ms) do ultrassom: repetiria a mesma medicao.
const unsigned long INTERVALO_DIST_MS = 200;

/* ─────────── BUSCA EM CIRCULO ───────────
 * Comportamento provisorio. Tres estados exclusivos no loop():
 *
 *   sem cone (modo 'p')          -> gira em circulo procurando
 *   cone centralizado            -> PARA
 *   cone visivel mas fora da     -> fuzzy assume (segue o cone e desvia)
 *   janela central
 *
 * AJUSTAR: a janela de +-50 no gradiente e chute confesso. O gradiente e
 * (cx - 320) * 255/320, entao 50 equivale a ~63 px num quadro de 640 -- cerca
 * de 10% da largura para cada lado do centro. Alargue se o robo ficar oscilando
 * sem nunca considerar centralizado; estreite se ele parar com o cone
 * visivelmente de lado.
 */
static const int JANELA_CENTRADO = 50;

/* AJUSTAR NA BANCADA: duty de tracao durante a busca. Tem que ser suficiente
 * para o robo descrever o circulo sem arrastar, e baixo o bastante para ele nao
 * sair de perto do cone quando passar por ele. */
static const int VEL_BUSCA = 170;

// Lado do circulo. +255 e esquerda (PIN_ESQ), -255 e direita.
static const int DIR_BUSCA = 255;

/* ─────────── ESTADO ─────────── */
volatile unsigned long ultimoComandoMs = 0;
volatile bool recebeuComando = false;   // separa "nunca recebeu" de "recebeu ha 0 ms"

/* Protocolo novo da camera: gradiente (-255 a +255, POSITIVO = cone a DIREITA)
 * e modo ("aproximar" / "contornar" / "procurar"). Guardamos so a inicial do
 * modo: String em estado compartilhado com a thread da Bridge e pedir problema. */
volatile int  gradienteCone = 0;
volatile char modoCamera    = '?';   // 'a', 'c', 'p'


// Ultima saida efetivamente aplicada. A telemetria imprime isto em vez de
// chamar defuzzify() de novo: sem link, o fuzzify() nao roda no ciclo e a
// composicao ficaria velha.
static int ultimaVel = 0;
static int ultimaDir = 0;


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

/* ═══════════ ESTADO DA BUSCA ═══════════ */

/* Cone centralizado exige a camera VENDO o cone, nao so gradiente pequeno: em
 * "procurar" o ia_trekking manda gradiente 0, que cairia na janela e faria o
 * robo parar justamente quando nao ha cone nenhum. */
bool coneCentralizado() {
  const int g = gradienteCone;
  return modoCamera != 'p' && g >= -JANELA_CENTRADO && g <= JANELA_CENTRADO;
}

bool procurandoCone() {
  return modoCamera == 'p';
}

/* ═══════════ TELEMETRIA ═══════════ */

void imprimirTelemetria() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < INTERVALO_LOG_MS) return;
  ultimo = millis();

  Serial.print("Camera: ");
  Serial.print(gradienteCone);
  Serial.print(' ');
  Serial.print((char)modoCamera);

  Serial.print(" | S1: "); Serial.print(dist_1, 1);
  Serial.print(" S2: ");   Serial.print(dist_2, 1);
  Serial.print(" S3: ");   Serial.print(dist_3, 1);
// Serial.print(" S4: ");   Serial.print(dist_4, 1);


  Serial.print(" | dir: "); Serial.print(ultimaDir);
  Serial.print(" vel: ");   Serial.print(ultimaVel);

  Serial.println();
}

/* ═══════════ ENVIO DAS DISTANCIAS ═══════════ */

/*
 * Entrega as distancias ao lado Linux. Isto JA NAO E LOG: a gravacao em
 * distancias.txt foi removida depois que a caracterizacao dos sensores ficou
 * pronta. Hoje o consumidor e o overlay do modo visual, que escreve
 * "F:.. D:.. E:.." por cima do video.
 *
 * A linha DIST; na serial sobrou do tempo do log por pipe e nao tem mais
 * consumidor automatico -- serve so para leitura humana no monitor.
 */

void registrarDistancias() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < INTERVALO_DIST_MS) return;
  const unsigned long agora = millis();
  ultimo = agora;

  /* Caminho principal: entrega ao lado Linux, que grava o .txt de dentro do
   * ia_trekking.py. notify() e "dispare e esqueca" -- se ninguem do outro lado
   * tiver registrado o metodo, a chamada nao bloqueia nem da erro, o robo segue.
   *
   * NAO chamar isto de dentro de um callback RPC (processa_direcao): a propria
   * biblioteca avisa que call/notify dentro de callback trava a IPC. Aqui
   * estamos no loop(), que e seguro. */
  Bridge.notify("registra_distancias", agora, dist_1, dist_2, dist_3);

  /* Caminho reserva: a mesma linha na serial, para leitura humana no monitor. */
  Serial.print("DIST;");
  Serial.print(agora);      Serial.print(';');
  Serial.print(dist_1, 1);  Serial.print(';');
  Serial.print(dist_2, 1);  Serial.print(';');
  Serial.println(dist_3, 1);
}


/* ═══════════ ESTERCO CONTINUO ═══════════ */


/* AJUSTAR NA BANCADA: menor duty que tira o esterco do centro contra a mola.
 * Abaixo disto o motor so consome corrente e esquenta, sem mover nada. */
static const int ESTERCO_MIN_UTIL = 90;


/* O fuzzy entrega duty continuo, entao nao passa por acionarEsterco() e perderia
 * o teto termico do motores.hpp. Este wrapper reaplica o mesmo teto: rotor
 * bloqueado contra a mola em duty alto e exatamente o caso que ele protege. */
void aplicarEstercoFuzzy(int duty) {
  static unsigned long ligadoDesde = 0;
  static unsigned long alivioAte   = 0;
  static bool          ligado      = false;

  const unsigned long agora = millis();
  const int modulo = (duty < 0) ? -duty : duty;

  if (modulo < ESTERCO_MIN_UTIL || agora < alivioAte) {
    ligado = false;
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
    return;
  }

  if (!ligado) { ligado = true; ligadoDesde = agora; }

  if (agora - ligadoDesde >= ESTERCO_MAX_MS) {
    alivioAte = agora + ESTERCO_ALIVIO_MS;
    ligado    = false;
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
    return;
  }

  acionarPonte(PIN_ESQ, PIN_DIR, duty);
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

  inicializarFuzzy();     // monta a base de regras UMA vez: a eFLL aloca com new

#if USAR_MPU
  inicializarMPU();
#endif
  inicializarLedModo();
}

void loop() {
  const bool fresco = comandoEstaFresco();

  atualizarSonares();   // antes do fuzzy: ele decide com o dado deste ciclo


  /* 'fresco' e a chave geral: sem ninguem mandando comando pela Bridge, o robo
   * fica parado. Serve de botao de emergencia -- matar o ia_trekking para o
   * robo em menos de um segundo. */
  int vel = 0, dir = 0;

  if (fresco) {
    if (coneCentralizado()) {
      // Achou e esta de frente. Comportamento provisorio: para aqui.
      vel = 0;
      dir = 0;
    }
    else if (procurandoCone()) {
      // Camera nao ve cone nenhum: gira em circulo ate aparecer.
      vel = VEL_BUSCA;
      dir = DIR_BUSCA;
    }
    else {
      /* Cone a vista mas fora da janela: o fuzzy assume, usando as tres
       * distancias e o gradiente. 999 (eco perdido) esta fora do universo
       * 0-400 e entra como "livre". */
      fuzzy->setInput(1, dist_2 >= INVALID_DISTANCE ? 400.0f : dist_2);  // direita
      fuzzy->setInput(2, dist_3 >= INVALID_DISTANCE ? 400.0f : dist_3);  // esquerda
      fuzzy->setInput(3, dist_1 >= INVALID_DISTANCE ? 400.0f : dist_1);  // frente
      fuzzy->setInput(4, (float)gradienteCone);                          // cone
      fuzzy->fuzzify();

      vel = (int)fuzzy->defuzzify(1);   // FuzzyOutput(1) = velocidade
      dir = (int)fuzzy->defuzzify(2);   // FuzzyOutput(2) = direcao, + = esquerda
    }
  }

  ultimaVel = vel;
  ultimaDir = dir;

  acionarPonte(PIN_FR, PIN_TR, vel);
  aplicarEstercoFuzzy(dir);




  registrarDistancias();

  // LED aceso = recebendo comando. Diagnostico de bancada sem serial.
  digitalWrite(LED_BUILTIN, fresco ? HIGH : LOW);
#if USAR_MPU
  lerMPU();
#endif
  atualizarLedModo(fresco, modoCamera);
  imprimirTelemetria();
}
