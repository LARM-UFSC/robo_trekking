/**
 * Firmware do MCU do Arduino Uno Q (STM32U585 / Zephyr).
 *
 * Recebe o comando de direcao da CPU Linux pela Bridge e aciona os motores.
 * Ultrassom e MPU6050 sao opcionais: se nao compilarem, desligue pelas
 * flags abaixo e o robo continua andando.
 *
 * Suba por partes:
 *   1) USAR_ULTRASSOM 0 e USAR_MPU 0  -> so Bridge + motores
 *   2) ligue o ultrassom
 *   3) ligue o MPU
 *
 * Organizacao (headers-only, todos incluidos so por este arquivo):
 *   motores.hpp    pinos, pontes H, tracao e esterco
 *   ultrassom.hpp  3 HC-SR04 com trigger unico   (gated por USAR_ULTRASSOM)
 *   desvio.hpp     desvio de obstaculo            (gated por USAR_ULTRASSOM)
 *   manobra.hpp    retorno em U ao chegar no cone
 *   mpu.hpp        MPU6050 via I2C               (gated por USAR_MPU)
 *
 * As flags precisam ser definidas ANTES dos includes: os headers de sensor
 * dependem delas para decidir se geram codigo.
 */

#define USAR_ULTRASSOM 1
#define USAR_MPU       0   // desligada por enquanto

/* Controlador do desvio: 1 = fuzzy (desvioFuzzy.hpp), 0 = discreto (desvio.hpp).
 * O caminho discreto continua inteiro no arquivo, para comparacao. */
#define USAR_FUZZY     1

/* Retorno em U desligado: menos variaveis no teste do fuzzy. Com 0, o estado de
 * aproximacao e a parada no cone tambem saem -- eles existem para disparar o U. */
#define USAR_RETORNO_U 0

#include <Arduino_RouterBridge.h>

#include "motores.hpp"
#include "ultrassom.hpp"
#include "desvio.hpp"
#include "desvioFuzzy.hpp"
#include "manobra.hpp"
#include "mpu.hpp"

/* ─────────── PARAMETROS ─────────── */
const unsigned long INTERVALO_LOG_MS = 300;

/*
 * Failsafe: tempo maximo sem comando novo antes de assumir que o link caiu.
 * A CPU Linux para de chamar processa_direcao quando a camera some (o laco do
 * ia_trekking.py sai antes do Bridge.call), e sem isto comandoCamera congela no
 * ultimo byte -- com 'F', 'L' ou 'R' a tracao fica ligada indefinidamente.
 *
 * AJUSTAR: use 3-5x o periodo entre comandos. O FPS sai no log do ia_trekking a
 * cada 30 frames; 800 ms cobre ate ~4 FPS. Se a visao ficar mais lenta que isso,
 * o failsafe passa a disparar em regime normal e o robo anda aos trancos.
 */
const unsigned long TIMEOUT_COMANDO_MS = 800;

/* ─────────── MISSAO: chegar no cone ───────────
 * O cone tambem e obstaculo para o ultrassom. Sem tratamento, o desvio contorna
 * o cone a 50 cm e o robo nunca chega nele. Por isso ha um estado de
 * aproximacao: com o cone A VISTA e perto, o desvio para de esterçar e o robo
 * segue a camera ate a distancia de parada.
 *
 * O ultrassom nao distingue cone de parede. Se a camera estiver vendo um cone
 * ao longe e houver uma parede perto, o robo vai ate a parede -- mas PARA nela,
 * a PARADA_CONE_CM, e faz o U. Ou seja: erra o alvo, nao bate.
 *
 * AJUSTAR: DIST_APROXIMACAO_CM deve ser igual ou maior que DIST_DESVIO_CM,
 * senao o desvio dispara antes da aproximacao assumir. */
static const float DIST_APROXIMACAO_CM = 50.0f;
static const float PARADA_CONE_CM      = 25.0f;

// A camera so emite 'F', 'L' ou 'R' quando ha cone detectado; 'S' e "nao vejo".
inline bool coneAVista(char cmd) {
  return cmd == 'F' || cmd == 'L' || cmd == 'R';
}

// Periodo da linha de log das distancias. Nao adianta ser menor que o
// INTERVALO_SONAR_MS (60 ms) do ultrassom: repetiria a mesma medicao.
const unsigned long INTERVALO_DIST_MS = 200;

/* ─────────── ESTADO ─────────── */
volatile char comandoCamera = 'S';
volatile unsigned long ultimoComandoMs = 0;
volatile bool recebeuComando = false;   // separa "nunca recebeu" de "recebeu ha 0 ms"

/* Protocolo novo da camera: gradiente (-255 a +255, POSITIVO = cone a DIREITA)
 * e modo ("aproximar" / "contornar" / "procurar"). Guardamos so a inicial do
 * modo: String em estado compartilhado com a thread da Bridge e pedir problema. */
volatile int  gradienteCone = 0;
volatile char modoCamera    = '?';   // 'a', 'c', 'p'

#if USAR_FUZZY
// Ultima saida efetivamente aplicada. A telemetria imprime isto em vez de
// chamar defuzzify() de novo: sem link, o fuzzify() nao roda no ciclo e a
// composicao ficaria velha.
static int ultimaVel = 0;
static int ultimaDir = 0;
#endif

/* ═══════════ BRIDGE (CPU Linux -> MCU) ═══════════ */

// Mesma assinatura do bridgemcu.ino, que ja funciona nesta placa.
// Roda em contexto proprio (thread "bridge" do Zephyr), por isso o estado
// compartilhado e volatile.
void processa_direcao(int gradiente, String modo) {
  gradienteCone = constrain(gradiente, -255, 255);
  modoCamera    = (modo.length() > 0) ? modo.charAt(0) : '?';

  /* Mantem o comando em letra vivo para o caminho discreto (USAR_FUZZY 0)
   * continuar funcionando sob o protocolo novo. */
  if      (gradienteCone < -60) comandoCamera = 'L';
  else if (gradienteCone >  60) comandoCamera = 'R';
  else                          comandoCamera = 'F';

  ultimoComandoMs = millis();
  recebeuComando  = true;
}

// Comando so vale enquanto for recente. Sem isto o robo segue o ultimo byte
// para sempre depois que o link cai.
bool comandoEstaFresco() {
  return recebeuComando && (millis() - ultimoComandoMs) < TIMEOUT_COMANDO_MS;
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
#if USAR_ULTRASSOM
  Serial.print(" | S1: "); Serial.print(dist_1, 1);
  Serial.print(" S2: ");   Serial.print(dist_2, 1);
  Serial.print(" S3: ");   Serial.print(dist_3, 1);
// Serial.print(" S4: ");   Serial.print(dist_4, 1);
#endif
#if USAR_FUZZY
  Serial.print(" | dir: "); Serial.print(ultimaDir);
  Serial.print(" vel: ");   Serial.print(ultimaVel);
#endif
  if (retornoUAtivo()) Serial.print(" [U]");
  if (emDesvio())      Serial.print(" [DESVIO]");
  Serial.println();
}

/* ═══════════ LOG DE DISTANCIAS ═══════════ */

/*
 * Linha de formato fixo para ser lida por script, separada da telemetria
 * humana pelo prefixo DIST;. Campos:
 *
 *   DIST;<millis>;<frente>;<direita>;<esquerda>
 *
 * O MCU nao tem sistema de arquivos util, entao quem grava o .txt e o
 * log_ultrassom.py do lado Linux, consumindo estas linhas.
 */
#if USAR_ULTRASSOM

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

  /* Caminho reserva: a mesma linha na serial, para o log_ultrassom.py via pipe
   * e para leitura humana no monitor. */
  Serial.print("DIST;");
  Serial.print(agora);      Serial.print(';');
  Serial.print(dist_1, 1);  Serial.print(';');
  Serial.print(dist_2, 1);  Serial.print(';');
  Serial.println(dist_3, 1);
}

#else
inline void registrarDistancias() {}   // ultrassom desligado: nada a registrar
#endif

/* ═══════════ ESTERCO CONTINUO (caminho fuzzy) ═══════════ */
#if USAR_FUZZY

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

#endif  // USAR_FUZZY

/* ═══════════ SETUP / LOOP ═══════════ */

void setup() {
  Serial.begin(115200);

  inicializarMotores();     // configura os pinos e ja deixa tudo parado

  Bridge.begin();
  Bridge.provide("processa_direcao", processa_direcao);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

#if USAR_ULTRASSOM
  inicializarSonares();
#endif
#if USAR_FUZZY
  inicializarFuzzy();     // monta a base de regras UMA vez: a eFLL aloca com new
#endif
#if USAR_MPU
  inicializarMPU();
#endif
}

void loop() {
  const bool fresco = comandoEstaFresco();

  // Link caido vira 'S': tracao e esterco soltos.
  const char cmdCamera = fresco ? comandoCamera : 'S';

#if USAR_ULTRASSOM
  atualizarSonares();   // antes do desvio: ele decide com o dado deste ciclo
#endif

#if USAR_FUZZY

  /* O fuzzy decide so pelos ultrassonicos. O comando da camera nao entra na
   * conta -- mas 'fresco' continua valendo como chave geral: sem ninguem
   * mandando comando pela Bridge, o robo fica parado. E o botao de emergencia
   * do teste, e o conteudo do byte e irrelevante, so a chegada dele. */
  int vel = 0, dir = 0;

  if (fresco) {
    // 999 (eco perdido) esta fora do universo 0-400: entra como "livre".
    fuzzy->setInput(1, dist_2 >= INVALID_DISTANCE ? 400.0f : dist_2);  // direita
    fuzzy->setInput(2, dist_3 >= INVALID_DISTANCE ? 400.0f : dist_3);  // esquerda
    fuzzy->setInput(3, dist_1 >= INVALID_DISTANCE ? 400.0f : dist_1);  // frente
    fuzzy->setInput(4, (float)gradienteCone);                          // cone
    fuzzy->fuzzify();

    vel = (int)fuzzy->defuzzify(1);   // FuzzyOutput(1) = velocidade
    dir = (int)fuzzy->defuzzify(2);   // FuzzyOutput(2) = direcao, + = esquerda
  }

  ultimaVel = vel;
  ultimaDir = dir;

  acionarPonte(PIN_FR, PIN_TR, vel);
  aplicarEstercoFuzzy(dir);

#else

  /* Maquina de estados discreta, em ordem de prioridade:
   *   1. manobra em curso  -> nada interrompe o U
   *   2. chegou no cone    -> para e inicia o U
   *   3. aproximando       -> camera manda, desvio nao esterça (o cone e o alvo)
   *   4. normal            -> ultrassom tem prioridade sobre a camera
   */
  char cmd;

#if USAR_RETORNO_U
  if (retornoUAtivo()) {
    cmd = passoRetornoU();
    if (cmd == 0) cmd = 'S';                    // terminou neste ciclo
  }
  else if (coneAVista(cmdCamera) && dist_1 <= PARADA_CONE_CM) {
    iniciarRetornoU();
    cmd = 'S';
  }
  else if (coneAVista(cmdCamera) && dist_1 <= DIST_APROXIMACAO_CM) {
    cmd = cmdCamera;
  }
  else
#endif
  {
    cmd = aplicarDesvio(cmdCamera);
  }

  aplicarComando(cmd);

#endif  // USAR_FUZZY

  registrarDistancias();

  // LED aceso = recebendo comando. Diagnostico de bancada sem serial.
  digitalWrite(LED_BUILTIN, fresco ? HIGH : LOW);
#if USAR_MPU
  lerMPU();
#endif
  imprimirTelemetria();
}
