#ifndef MPU_HPP
#define MPU_HPP

/**
 * MPU6050 via I2C: aceleracao, giro e inclinacao.
 *
 * I2C do MPU: D20 = PB11 (I2C2_SDA), D21 = PB10 (I2C2_SCL).
 * Wire.begin() usa o barramento padrao; se o MPU nao for encontrado, tente
 * Wire1 (ver README).
 *
 * Header-only, ativado por USAR_MPU. A flag e definida no firmware_unoq.ino
 * ANTES deste include -- com ela em 0, nem as bibliotecas Adafruit sao
 * incluidas, e o sketch compila sem elas.
 */

#include <Arduino.h>

#ifndef USAR_MPU
  #error "Defina USAR_MPU antes de incluir mpu.hpp"
#endif

#if USAR_MPU

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

static const unsigned long INTERVALO_MPU_MS = 200;

static Adafruit_MPU6050 mpu;
static bool mpuOk = false;

inline void inicializarMPU() {
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

inline void lerMPU() {
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
#endif  // MPU_HPP
