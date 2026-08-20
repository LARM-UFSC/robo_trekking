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

#define USAR_ULTRASSOM 1
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

// 3 sensores na frente, 1 atras.
#define TRIG_1   2    // PB3
#define ECHO_1   4    // PA12   frente
#define TRIG_2   5    // PA11
#define ECHO_2   6    // PB1    frente
#define TRIG_3   7    // PB2
#define ECHO_3   8    // PB4    frente
// Sensor 4 (traseiro) desativado por enquanto. Pinos reservados.
// #define TRIG_4  12    // PB14
// #define ECHO_4  13    // PB13   traseiro

// I2C do MPU: D20 = PB11 (I2C2_SDA), D21 = PB10 (I2C2_SCL)

/* ─────────── PARAMETROS ─────────── */
const int dutyTracao  = 178;   // 0-255
const int dutyDirecao = 178;

const unsigned long TEMPO_ESTERCO_MS   = 250;
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

void aplicarComando(char cmd) {
  static char ultimoCmd = 'S';
  static unsigned long inicioEsterco = 0;

  if (cmd != ultimoCmd) {
    ultimoCmd = cmd;
    inicioEsterco = millis();
  }

  if (cmd == 'F' || cmd == 'L' || cmd == 'R') {
    acionarPonte(PIN_FR, PIN_TR, dutyTracao);
  } else {
    acionarPonte(PIN_FR, PIN_TR, 0);
  }

  const unsigned long CICLO = TEMPO_ESTERCO_MS * 2;
  bool faseAtiva = ((millis() - inicioEsterco) % CICLO) < TEMPO_ESTERCO_MS;

  if (!faseAtiva) {
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
  } else if (cmd == 'L') {
    acionarPonte(PIN_ESQ, PIN_DIR,  dutyDirecao);
  } else if (cmd == 'R') {
    acionarPonte(PIN_ESQ, PIN_DIR, -dutyDirecao);
  } else {
    acionarPonte(PIN_ESQ, PIN_DIR, 0);
  }
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
  const int trigs[] = {TRIG_1, TRIG_2, TRIG_3 /*, TRIG_4 */};
  const int echos[] = {ECHO_1, ECHO_2, ECHO_3 /*, ECHO_4 */};
  for (unsigned i = 0; i < sizeof(trigs) / sizeof(trigs[0]); i++) {
    pinMode(trigs[i], OUTPUT);
    digitalWrite(trigs[i], LOW);
    pinMode(echos[i], INPUT);
  }
}

// Nao usa pulseIn: so digitalRead e micros(), que existem em qualquer core.
unsigned long medirEcho(int pinEcho, unsigned long timeout_us) {
  unsigned long t0 = micros();
  while (digitalRead(pinEcho) == LOW) {
    if (micros() - t0 > timeout_us) return 0;
  }
  unsigned long inicio = micros();
  while (digitalRead(pinEcho) == HIGH) {
    if (micros() - inicio > timeout_us) return 0;
  }
  return micros() - inicio;
}

float lerSonar(int pinTrig, int pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);

  unsigned long us = medirEcho(pinEcho, ECHO_TIMEOUT_US);
  if (us == 0) return INVALID_DISTANCE;

  float cm = us / 58.0f;
  if (cm < 4.0f || cm > 400.0f) return INVALID_DISTANCE;
  return cm;
}

// Um sensor por vez, em rodizio, para nao bloquear os motores.
void atualizarSonares() {
  static unsigned long ultimo = 0;
  static int atual = 0;

  if (millis() - ultimo < INTERVALO_SONAR_MS) return;
  ultimo = millis();

  switch (atual) {
    case 0: dist_1 = lerSonar(TRIG_1, ECHO_1); break;
    case 1: dist_2 = lerSonar(TRIG_2, ECHO_2); break;
    case 2: dist_3 = lerSonar(TRIG_3, ECHO_3); break;
 // case 3: dist_4 = lerSonar(TRIG_4, ECHO_4); break;
  }
  atual = (atual + 1) % 3;   // volta para 4 ao reativar o traseiro
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
