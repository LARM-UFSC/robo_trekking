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
 *   mpu.hpp        MPU6050 via I2C               (gated por USAR_MPU)
 *
 * As flags precisam ser definidas ANTES dos includes: os headers de sensor
 * dependem delas para decidir se geram codigo.
 */

#define USAR_ULTRASSOM 0
#define USAR_MPU       0   // desligada por enquanto

#include <Arduino_RouterBridge.h>

#include "motores.hpp"
#include "ultrassom.hpp"
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
  Serial.println();
}

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
  aplicarComando(fresco ? comandoCamera : 'S');

  // LED aceso = recebendo comando. Diagnostico de bancada sem serial.
  digitalWrite(LED_BUILTIN, fresco ? HIGH : LOW);
#if USAR_ULTRASSOM
  atualizarSonares();
#endif
#if USAR_MPU
  lerMPU();
#endif
  imprimirTelemetria();
}
