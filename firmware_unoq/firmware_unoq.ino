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
 */

#define USAR_ULTRASSOM 0
#define USAR_MPU       0   // desligada por enquanto

#include <Arduino_RouterBridge.h>

#if USAR_MPU
  #include <Wire.h>
  #include <Adafruit_MPU6050.h>
  #include <Adafruit_Sensor.h>
  #include <math.h>
#endif

/* ─────────── PINOS (datasheet ABX00162, secao 9.6) ───────────
 * Motores nos pinos com ~ (PWM por hardware).
 * D5 e D6 so como digital: ha relato de analogWrite nao gerar PWM neles.
 */
#define PIN_FR   9    // PB8  TIM4_CH3 — tracao frente
#define PIN_TR  10    // PB9  TIM4_CH4 — tracao tras
#define PIN_ESQ  3    // PB0  TIM3_CH3 — direcao esquerda
#define PIN_DIR 11    // PB15 TIM1_CH3N — direcao direita

// 3 sensores na frente, 1 atras. TRIGGER unico, comum a todos.
#define PIN_TRIGGER 2    // PB3   disparo compartilhado
#define ECHO_1      4    // PA12  frente
#define ECHO_2      6    // PB1   frente
#define ECHO_3      8    // PB4   frente
// Sensor 4 (traseiro) desativado por enquanto. Pino reservado.
// #define ECHO_4  13    // PB13  traseiro
// Livres apos unificar o trigger: D5, D7, D12

// I2C do MPU: D20 = PB11 (I2C2_SDA), D21 = PB10 (I2C2_SCL)

/* ─────────── PARAMETROS ─────────── */
const int dutyTracao  = 178;   // 0-255
const int dutyRe      = 140;   // re mais devagar que a marcha a frente
const int dutyDirecao = 120;   // AJUSTAR NA BANCADA: ver comentario de acionarEsterco

const unsigned long ESTERCO_MAX_MS     = 2500;  // teto de acionamento continuo
const unsigned long ESTERCO_ALIVIO_MS  = 500;   // descanso obrigatorio apos o teto
const unsigned long INTERVALO_SONAR_MS = 60;
const unsigned long INTERVALO_MPU_MS   = 200;
const unsigned long INTERVALO_LOG_MS   = 300;

#define INVALID_DISTANCE 999.0f
#define ECHO_TIMEOUT_US  25000UL   // ~4 m ida e volta

/* ─────────── ESTADO ─────────── */
volatile char comandoCamera = 'S';

#if USAR_ULTRASSOM
float dist_1 = INVALID_DISTANCE, dist_2 = INVALID_DISTANCE;
float dist_3 = INVALID_DISTANCE;
// float dist_4 = INVALID_DISTANCE;   // traseiro
#endif

#if USAR_MPU
Adafruit_MPU6050 mpu;
static bool mpuOk = false;
#endif

/* ═══════════ MOTORES ═══════════ */

// duty > 0 aciona pinoA | duty < 0 aciona pinoB | 0 solta os dois
void acionarPonte(int pinoA, int pinoB, int duty) {
  if (duty > 0) {
    analogWrite(pinoB, 0);
    analogWrite(pinoA, duty);
  } else if (duty < 0) {
    analogWrite(pinoA, 0);
    analogWrite(pinoB, -duty);
  } else {
    analogWrite(pinoA, 0);
    analogWrite(pinoB, 0);
  }
}

void pararMotores() {
  acionarPonte(PIN_FR,  PIN_TR,  0);
  acionarPonte(PIN_ESQ, PIN_DIR, 0);
}

void inicializarMotores() {
  pinMode(PIN_FR,  OUTPUT);   pinMode(PIN_TR,  OUTPUT);
  pinMode(PIN_ESQ, OUTPUT);   pinMode(PIN_DIR, OUTPUT);
  pararMotores();
}

int estercoPedido(char cmd) {
  if (cmd == 'L') return  dutyDirecao;
  if (cmd == 'R') return -dutyDirecao;
  return 0;                                 // 'F', 'B' e 'S': esterco solto
}

/*
 * O esterco tem mola de centragem: solto, ele volta ao centro sozinho. Logo a
 * deflexao so se sustenta com o motor acionado, e o duty define o ponto de
 * equilibrio entre o torque do motor e a mola -- o duty E o comando de posicao.
 *
 * Por isso NAO ha mais pulso de 250 ms: aquilo era um PWM de 2 Hz por cima do
 * PWM real e fazia o esterco oscilar entre a deflexao e o centro, em vez de
 * segurar o angulo pedido. Ajuste o angulo por dutyDirecao, nao por tempo: se
 * o esterco encosta no batente, o duty esta alto demais e so gera corrente.
 *
 * Sobra o risco termico: segurar angulo contra a mola e rotor bloqueado, ainda
 * que longe do batente. Controlamos por duty (corrente) e por um teto de tempo
 * ligado, com alivio obrigatorio depois.
 */
void acionarEsterco(char cmd) {
  static int  ladoAnterior = 0;
  static unsigned long ligadoDesde = 0;
  static unsigned long alivioAte   = 0;

  const unsigned long agora = millis();
  const int lado = estercoPedido(cmd);

  // trocar de lado tira o motor do batente: o acumulado recomeca
  if (lado != ladoAnterior) {
    ladoAnterior = lado;
    ligadoDesde  = agora;
  }

  if (lado == 0 || agora < alivioAte) {
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
    return;
  }

  if (agora - ligadoDesde >= ESTERCO_MAX_MS) {   // teto atingido: alivia
    alivioAte   = agora + ESTERCO_ALIVIO_MS;
    ligadoDesde = alivioAte;
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
    return;
  }

  acionarPonte(PIN_ESQ, PIN_DIR, lado);
}

void aplicarComando(char cmd) {
  if (cmd == 'F' || cmd == 'L' || cmd == 'R') {
    acionarPonte(PIN_FR, PIN_TR, dutyTracao);
  } else if (cmd == 'B') {
    acionarPonte(PIN_FR, PIN_TR, -dutyRe);   // re
  } else {
    acionarPonte(PIN_FR, PIN_TR, 0);         // 'S' e qualquer byte estranho
  }

  acionarEsterco(cmd);
}

/* ═══════════ BRIDGE (CPU Linux -> MCU) ═══════════ */

// Mesma assinatura do bridgemcu.ino, que ja funciona nesta placa.
// Roda em contexto proprio, por isso comandoCamera e volatile.
void processa_direcao(String direcao) {
  if (direcao.length() > 0) {
    comandoCamera = direcao.charAt(0);
  }
}

/* ═══════════ ULTRASSOM ═══════════ */
#if USAR_ULTRASSOM

void inicializarSonares() {
  pinMode(PIN_TRIGGER, OUTPUT);
  digitalWrite(PIN_TRIGGER, LOW);

  const int echos[] = {ECHO_1, ECHO_2, ECHO_3 /*, ECHO_4 */};
  for (unsigned i = 0; i < sizeof(echos) / sizeof(echos[0]); i++) {
    pinMode(echos[i], INPUT);
  }
}

float usParaCm(unsigned long us) {
  if (us == 0) return INVALID_DISTANCE;
  float cm = us / 58.0f;
  if (cm < 4.0f || cm > 400.0f) return INVALID_DISTANCE;
  return cm;
}

/*
 * Trigger unico: um disparo faz os 3 sensores emitirem juntos, entao os 3
 * ecos precisam ser medidos do MESMO disparo. Fazemos isso varrendo os
 * pinos num laco, registrando subida e descida de cada um.
 * Nao usa pulseIn: so digitalRead e micros().
 */
void atualizarSonares() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < INTERVALO_SONAR_MS) return;
  ultimo = millis();

  const int   echos[] = {ECHO_1, ECHO_2, ECHO_3 /*, ECHO_4 */};
  const int   N       = sizeof(echos) / sizeof(echos[0]);
  unsigned long inicio[N], largura[N];
  bool subiu[N], pronto[N];

  for (int i = 0; i < N; i++) {
    inicio[i] = 0; largura[i] = 0;
    subiu[i]  = false; pronto[i] = false;
  }

  // dispara uma unica vez
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  unsigned long t0 = micros();
  int faltam = N;

  while (faltam > 0 && (micros() - t0) < ECHO_TIMEOUT_US) {
    for (int i = 0; i < N; i++) {
      if (pronto[i]) continue;
      int nivel = digitalRead(echos[i]);

      if (!subiu[i]) {
        if (nivel == HIGH) { subiu[i] = true; inicio[i] = micros(); }
      } else if (nivel == LOW) {
        largura[i] = micros() - inicio[i];
        pronto[i]  = true;
        faltam--;
      }
    }
  }

  dist_1 = pronto[0] ? usParaCm(largura[0]) : INVALID_DISTANCE;
  dist_2 = pronto[1] ? usParaCm(largura[1]) : INVALID_DISTANCE;
  dist_3 = pronto[2] ? usParaCm(largura[2]) : INVALID_DISTANCE;
// dist_4 = pronto[3] ? usParaCm(largura[3]) : INVALID_DISTANCE;
}

  #endif  // USAR_ULTRASSOM

/* ═══════════ MPU6050 ═══════════ */
#if USAR_MPU

void inicializarMPU() {
  Wire.begin();     // se nao achar o MPU, tente Wire1 (ver README)

  const unsigned long TIMEOUT_MS = 5000;
  unsigned long inicio = millis();

  while (millis() - inicio < TIMEOUT_MS) {
    if (mpu.begin()) { mpuOk = true; break; }
    Serial.println("MPU6050 nao respondeu, tentando de novo...");
    delay(250);
  }

  if (!mpuOk) { Serial.println("Sem MPU - seguindo sem IMU."); return; }

  Serial.println("MPU6050 encontrado!");
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void lerMPU() {
  if (!mpuOk) return;

  static unsigned long ultimo = 0;
  if (millis() - ultimo < INTERVALO_MPU_MS) return;
  ultimo = millis();

  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float gx = accel.acceleration.x / 9.81f;
  float gy = accel.acceleration.y / 9.81f;
  float gz = accel.acceleration.z / 9.81f;

  float roll  = atan2f(gy, gz) * 180.0f / M_PI;
  float pitch = atan2f(-gx, sqrtf(gy * gy + gz * gz)) * 180.0f / M_PI;

  Serial.print("Roll: ");   Serial.print(roll, 1);
  Serial.print(" Pitch: "); Serial.println(pitch, 1);
}
#endif  // USAR_MPU

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
  aplicarComando(comandoCamera);
#if USAR_ULTRASSOM
  atualizarSonares();
#endif
#if USAR_MPU
  lerMPU();
#endif
  imprimirTelemetria();
}
                                                                                                                    