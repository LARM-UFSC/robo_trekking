# ESP32

Este diretório contém o firmware do módulo ESP32 usado no projeto de trekking para leitura de sensores e leitura de comandos recebidos via comunicação serial.

## O que o firmware faz

O código realiza as seguintes tarefas:

- lê 4 sensores ultrassônicos
- recebe um comando de câmera via Serial2
- armazena os valores das leituras em variáveis internas
- imprime no monitor serial os valores de câmera e dos sensores de forma simples

## Funcionalidades principais

- leitura dos sensores ultrassônicos com a biblioteca NewPing
- comunicação serial com outro módulo ou computador
- uso de uma tarefa dedicada para coletar leituras dos sensores em paralelo
- saída simples e direta para depuração via Serial

## Arquivos

- `main.cpp`
  - contém o firmware principal
  - inicializa os pinos, a comunicação serial e os sensores
  - executa a leitura dos sensores em uma task separada
  - imprime os valores no monitor serial

## Dependências

Para compilar e usar este firmware, são necessárias:

- Arduino IDE ou PlatformIO
- placa compatível com ESP32
- biblioteca `NewPing`

## Como compilar e enviar

1. abra o arquivo `main.cpp` em um ambiente compatível com ESP32
2. instale a biblioteca `NewPing`
3. selecione a placa correta, como `ESP32 Dev Module`
4. compile e faça o upload para o ESP32

## Como funciona

1. o ESP32 inicializa a comunicação serial e os pinos usados pelos sensores
2. uma task dedicada lê continuamente os sensores ultrassônicos
3. o valor recebido pela câmera é lido pela Serial2
4. o programa imprime no monitor serial os dados coletados

## Observações

- o firmware foi deixado em uma versão simples, focada apenas na leitura e exibição dos dados
- os valores de distância são mostrados em centímetros
- o comando da câmera é exibido junto com um valor numérico simplificado para facilitar a depuração
