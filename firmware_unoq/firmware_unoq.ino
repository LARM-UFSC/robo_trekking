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

#include <Arduino_RouterBridge.h>

#include "motores.hpp"
#include "ultrassom.hpp"
#include "desvio.hpp"
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

/* ═══════════ BRIDGE (CPU Linux -> MCU) ═══════════ */

// Mesma assinatura do bridgemcu.ino, que ja funciona nesta placa.
// Roda em contexto proprio (thread "bridge" do Zephyr), por isso o estado
// compartilhado e volatile.
void processa_direcao(String direcao) {
  if (direcao.length() > 0) {
    comandoCamera   = direcao.charAt(0);
    ultimoComandoMs = millis();
    recebeuComando  = true;
  }
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
  Serial.print((char)comandoCamera);
#if USAR_ULTRASSOM
  Serial.print(" | S1: "); Serial.print(dist_1, 1);
  Serial.print(" S2: ");   Serial.print(dist_2, 1);
  Serial.print(" S3: ");   Serial.print(dist_3, 1);
// Serial.print(" S4: ");   Serial.print(dist_4, 1);
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

  /* Maquina de estados, em ordem de prioridade:
   *   1. manobra em curso  -> nada interrompe o U
   *   2. chegou no cone    -> para e inicia o U
   *   3. aproximando       -> camera manda, desvio nao esterça (o cone e o alvo)
   *   4. normal            -> ultrassom tem prioridade sobre a camera
   */
  char cmd;

  if (retornoUAtivo()) {
    cmd = passoRetornoU();
    if (cmd == 0) cmd = 'S';                    // terminou neste ciclo
  }
#if USAR_ULTRASSOM
  else if (coneAVista(cmdCamera) && dist_1 <= PARADA_CONE_CM) {
    iniciarRetornoU();
    cmd = 'S';
  }
  else if (coneAVista(cmdCamera) && dist_1 <= DIST_APROXIMACAO_CM) {
    cmd = cmdCamera;
  }
#endif
  else {
    cmd = aplicarDesvio(cmdCamera);
  }

  aplicarComando(cmd);

  registrarDistancias();

  // LED aceso = recebendo comando. Diagnostico de bancada sem serial.
  digitalWrite(LED_BUILTIN, fresco ? HIGH : LOW);
#if USAR_MPU
  lerMPU();
#endif
  imprimirTelemetria();
}
