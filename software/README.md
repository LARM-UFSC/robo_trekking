# Trekking UFSC -  Software

Este repositório reúne os componentes de software do projeto Trekking UFSC, com foco em percepção visual, comunicação embarcada e controle do robô. O objetivo principal é desenvolver um sistema capaz de identificar elementos do ambiente (no momento, apenas o cone), gerar comandos de direção e interagir com os módulos de hardware responsáveis por leitura de sensores e acionamento dos motores.

## Visão geral

O software do projeto é dividido em três partes principais:

- Visão computacional: usa modelos de detecção/segmentação para identificar objetos e inferir comandos.
- Firmware embarcado: executado em módulos Arduino Uno Q e ESP32 para comunicação e leitura de sensores.
- Integração entre módulos: o fluxo de dados vai da câmera para o modelo, depois para o controle e, por fim, para o hardware.

## Estrutura do repositório

```text
trekking_ufsc/
├── ArduinoUnoQ/
│   ├── bridgemcu.ino
│   ├── ia_trekking.py
│   └── ComandosArduinoQ
├── ESP32/
│   └── firmware_esp32/     # projeto PlatformIO
│       ├── platformio.ini
│       ├── include/
│       └── src/
├── VisãoComputacional/
│   ├── Conversao.py
│   ├── Treiner.py
│   └── README.md
└── README.md
```

## Módulos de software

### 1. Arduino Uno Q

A pasta ArduinoUnoQ contém o firmware e o script de inferência usados para integrar a lógica de direção com o hardware.

- bridgemcu.ino: firmware responsável por receber comandos e repassá-los para o módulo de controle via serial.
- ia_trekking.py: script em Python que usa YOLO para processar imagens da câmera, identificar objetos e enviar comandos como esquerda, direita, frente e parar.

### 2. ESP32

A pasta ESP32 contém o firmware responsável pela leitura dos sensores, pelo acionamento dos motores e pela comunicação com o restante do sistema. É um projeto PlatformIO, em `firmware_esp32/`, organizado em nodes:

- ultrassom_node: lê os 4 sensores HC-SR04 em uma task dedicada e recebe o comando da câmera pela Serial2.
- mpu_node: lê o MPU6050 (aceleração, rotação e inclinação) via I2C.
- motores_node: traduz o comando recebido em acionamento dos motores de tração e direção, via PWM nas pontes H.

Detalhes de pinagem, protocolo e limitações estão no README da pasta ESP32.

### 3. Visão computacional

A pasta VisãoComputacional contém os arquivos usados para preparar e treinar o modelo de IA.

- Conversao.py: converte anotações de dataset para o formato esperado pelo YOLO.
- Treiner.py: treina o modelo e prepara a exportação para inferência.

## Fluxo de funcionamento

O fluxo de software do projeto pode ser descrito assim:

1. A câmera captura imagens do ambiente.
2. O modelo de visão computacional identifica objetos relevantes.
3. O script Python gera um comando de direção, como L, R, F ou S.
4. O comando é enviado para o firmware do Arduino Uno Q.
5. O firmware repassa o comando para o ESP32 via Serial.
6. O ESP32 aciona os motores conforme o comando e coleta os dados dos sensores ultrassônicos e da IMU.

## Requisitos

### Python

Dependências principais:

```bash
pip install opencv-python ultralytics
```

Além disso, o script Python da pasta ArduinoUnoQ depende do pacote arduino.app_utils.

### Firmware embarcado

- PlatformIO, para o firmware do ESP32
- Arduino IDE ou Arduino CLI, para o firmware do Arduino Uno Q
- Placa compatível com Arduino Uno Q e ESP32

As bibliotecas do ESP32 (NewPing e Adafruit MPU6050) estão declaradas em `platformio.ini` e não precisam ser instaladas à mão.

## Como executar o software

### 1. Preparar o ambiente Python

Instale as dependências necessárias:

```bash
cd ArduinoUnoQ
pip install opencv-python ultralytics
```

### 2. Compilar e enviar o firmware Arduino - NO AMBIENTE DO ARDUINO

Use o firmware da pasta ArduinoUnoQ para carregar o código no módulo correspondente.

### 3. Compilar e enviar o firmware ESP32 - NO COMPUTADOR

O firmware é um projeto PlatformIO. As bibliotecas são resolvidas automaticamente:

```bash
cd ESP32/firmware_esp32
pio run -t upload
pio device monitor
```

### 4. Treinar ou preparar o modelo de visão - NO COMPUTADOR

Caso seja necessário treinar um novo modelo, siga o fluxo descrito na pasta VisãoComputacional.

### 5. Executar a inferência - NO AMBIENTE DO ARDUINO

Execute o script Python da pasta ArduinoUnoQ:

```bash
python ia_trekking.py
```

## Observações importantes

- O projeto está em fase de desenvolvimento e o software pode passar por ajustes conforme o hardware e os testes evoluem.
- A parte de visão computacional é um ponto central do sistema, pois define a interpretação do ambiente.
- A comunicação entre os módulos embarcados é essencial para o funcionamento integrado do robô.

## Próximos passos sugeridos

- Melhorar a robustez da inferência visual.
- Ajustar a lógica de decisão para diferentes cenários.
- Integrar melhor os dados dos sensores ultrassônicos com os comandos de navegação.
- Documentar os fluxos de teste e validação do sistema.
