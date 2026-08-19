# ESP32

Este diretório contém o firmware do módulo ESP32 do robô de trekking. Ele lê os
sensores de bordo, recebe os comandos de direção vindos da câmera e aciona os
motores.

O firmware é um projeto **PlatformIO** com framework **Arduino**, em
`firmware_esp32/`. A placa alvo é a `esp32dev`.

## O que o firmware faz

- lê 4 sensores ultrassônicos HC-SR04 em uma task dedicada
- lê o MPU6050 (acelerômetro, giroscópio e inclinação) via I2C
- recebe o comando da câmera por `Serial2`
- aciona os motores de tração e de direção conforme o comando
- publica telemetria no monitor serial para depuração

## Estrutura

```text
firmware_esp32/
├── platformio.ini
├── include/
│   ├── motores_node.hpp
│   ├── mpu_node.hpp
│   └── ultrassom_node.hpp
└── src/
    ├── main.cpp            # setup() e loop()
    ├── motores_node.cpp    # pontes H, PWM e tradução do comando
    ├── mpu_node.cpp        # MPU6050 via I2C
    └── ultrassom_node.cpp  # 4 HC-SR04 e leitura do Serial2
```

Cada "node" é um par `.cpp` em `src/` mais `.hpp` em `include/`, com o estado
compartilhado exposto via `extern`.

## Dependências

Declaradas em `platformio.ini` e instaladas automaticamente pelo PlatformIO:

- `teckel12/NewPing`
- `adafruit/Adafruit MPU6050`
- `adafruit/Adafruit Unified Sensor`
- `adafruit/Adafruit BusIO`

## Como compilar e enviar

```bash
cd firmware_esp32
pio run                 # compila
pio run -t upload       # grava no ESP32
pio device monitor      # monitor serial
```

A porta e a velocidade estão fixadas em `platformio.ini`: `/dev/ttyACM0` e
115200 baud. Ajuste conforme a sua máquina.

## Mapa de pinos

| Função | GPIO |
|---|---|
| Motor direção — esquerda / direita | 23 / 4 |
| Motor tração — frente / trás | 13 / 32 |
| Ultrassom TRIGGER (comum aos 4) | 26 |
| ECHO 1 (frente-direita) | 25 |
| ECHO 2 (frente-esquerda) | 27 |
| ECHO 3 (trás-direita) | 33 |
| ECHO 4 (trás-esquerda) | 34 |
| I2C SDA / SCL (MPU6050) | 21 / 22 |
| UART2 RX / TX (câmera) | 16 / 14 |
| LED azul | 2 |
| LED de inclinação | 19 |

Os quatro canais PWM (LEDC 0–3) usam 5 kHz e 8 bits de resolução.

## Protocolo da câmera

Um byte em `Serial2`, enviado pelo Arduino Uno Q:

| Byte | Significado | Tração | Direção |
|---|---|---|---|
| `'F'` | frente | ligada | centro |
| `'L'` | esquerda | ligada | esquerda |
| `'R'` | direita | ligada | direita |
| `'S'` | parar | desligada | centro |

Qualquer outro byte é tratado como `'S'`.

## Como funciona

- **Core 0** — `TaskSensores` dispara os 4 sonares em sequência e grava em
  `dist_1..dist_4`. Leitura inválida ou abaixo de 4 cm vira `INVALID_DISTANCE`.
- **Core 1 (loop)** — lê o byte da câmera, aciona os motores, imprime a
  telemetria e lê o MPU6050 a 5 Hz.

O motor de direção é DC, sem realimentação de posição. Por isso o esterço é
**pulsado** (250 ms acionado, 250 ms solto): acionamento contínuo o levaria ao
batente mecânico e o deixaria travado, com corrente de rotor bloqueado. Quem
fecha a malha de posição é a câmera, que observa o resultado e corrige o
comando no frame seguinte.

Se o MPU6050 não responder, o firmware tenta por até 5 segundos e segue sem
IMU, em vez de travar o boot.

## Estado atual e limitações

- O acionamento dos motores **ainda não foi validado no hardware**. Faça o
  primeiro teste com o robô suspenso, conferindo sentido de giro e o
  comportamento do esterço.
- Não há timeout no comando da câmera: se a comunicação cair, o último comando
  vale indefinidamente. Antes de testar com o robô no chão, tenha um timeout ou
  uma chave física na alimentação dos motores.
- As distâncias dos ultrassônicos são publicadas mas ainda não entram na
  decisão de navegação.
