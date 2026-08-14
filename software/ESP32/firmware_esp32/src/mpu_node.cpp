/**
 * MPU6050 ESP32 Node — Arduino + Adafruit MPU6050
 *
 * Lê acelerômetro, giroscópio e temperatura.
 * Saída em unidades fáceis de entender.
 */

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

/* ── Pinos I2C (padrão ESP32) ── */
#define SDA_PIN 21
#define SCL_PIN 22

#define LOOP_HZ 5


#define LED 19

Adafruit_MPU6050 mpu;


void InicializarMPU(void)
{
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mpu.begin()) {
        Serial.println("Falha ao encontrar o MPU6050!");
        while (1) { delay(10); }
    }
    Serial.println("MPU6050 encontrado!");

    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("Configuração concluída.\n");
    Serial.println("=== Legenda ===");
    Serial.println("Força G  : aceleração em cada eixo (1.00 = gravidade, 0.00 = parado)");
    Serial.println("Rotação  : velocidade de giro em graus/s (0 = sem rotação)");
    Serial.println("Inclinação: ângulo de inclinação em graus (0° = plano, 90° = vertical)");
    Serial.println("================\n");

    pinMode(LED, OUTPUT);
}

void LeituraMPU(void)
{
    static unsigned long last_ms = 0;
    unsigned long now = millis();

    if (now - last_ms < (1000 / LOOP_HZ)) return;
    last_ms = now;

    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    /* Aceleração em G (1G = gravidade terrestre) */
    float gx = accel.acceleration.x / 9.81f;
    float gy = accel.acceleration.y / 9.81f;
    float gz = accel.acceleration.z / 9.81f;

    /* Rotação em graus por segundo */
    float rx = gyro.gyro.x * RAD_TO_DEG;
    float ry = gyro.gyro.y * RAD_TO_DEG;
    float rz = gyro.gyro.z * RAD_TO_DEG;

    /* Inclinação em graus (calculada pelo acelerômetro) */
    float roll  = atan2f(gy, gz) * 180.0f / M_PI;
    float pitch = atan2f(-gx, sqrtf(gy * gy + gz * gz)) * 180.0f / M_PI;

    Serial.printf("Força G: X=%+5.2f  Y=%+5.2f  Z=%+5.2f | "
                  "Rotação(°/s): X=%+6.1f Y=%+6.1f Z=%+6.1f | "
                  "Inclinação: Roll=%+6.1f° Pitch=%+6.1f° | "
                  "Temp: %.1f°C\n",
                  gx, gy, gz,
                  rx, ry, rz,
                  roll, pitch,
                  temp.temperature);
    
    if(roll >= 30){
        digitalWrite(LED, HIGH);
    }
    else{
        digitalWrite(LED, LOW);
    }
}

void CalibrarSensor()
{
    
}
