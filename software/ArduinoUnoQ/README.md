# ArduinoUnoQ

Este diretório contém o firmware e os comandos de suporte para a unidade Arduino Uno Q usada no projeto de trekking.

## Arquivos

- `bridgemcu.ino`
  - Firmware Arduino para o MCU-ArduinoQ.
  - Inicializa a ponte `Bridge` e expõe a função `processa_direcao`.
  - Recebe comandos de direção (`L`, `R`, `F`, `S`) e os envia via `Serial1` para o módulo de controle.
  - Acende o LED onboard enquanto processa cada comando.

- `ia_trekking.py`
  - Script Python que usa `ultralytics.YOLO` para segmentação e detecção de objeto.
  - Captura frames de câmera, detecta cones e calcula o comando de direção com base na posição do objeto.
  - Envia o comando ao MCU-ArduinoQ por meio da `Bridge` usando `Bridge.call("processa_direcao", comando)`.
  - Registra FPS e tenta reenviar `S` ao encerrar com `KeyboardInterrupt`.

- `ComandosArduinoQ`
  - Arquivo de referência com comandos úteis para compilar e enviar firmware, além de controlar o serviço Python no dispositivo.
  - Contém instruções de `arduino-cli`, `systemctl`, `ssh` e `scp`.

## Dependências

- Arduino: `arduino-cli` configurado para o pacote `arduino:zephyr:unoq`.
- Python: `opencv-python`, `ultralytics`, e o pacote `arduino.app_utils` disponíveis no ambiente Python.
- Modelo YOLO: o diretório `best_ncnn_model` deve existir e conter o modelo treinado.

## Como compilar e enviar o firmware

```bash
arduino-cli compile --fqbn arduino:zephyr:unoq ~/trekking_ufsc/bridgemcu
arduino-cli upload -p internal --fqbn arduino:zephyr:unoq ~/trekking_ufsc/bridgemcu
```

## Gerenciar o serviço Python

- Recarregar daemon: `sudo systemctl daemon-reload`
- Iniciar serviço: `sudo systemctl start ia_trekking.service`
- Parar serviço: `sudo systemctl stop ia_trekking.service`
- Verificar status: `systemctl status ia_trekking.service`
- Ver logs em tempo real: `journalctl -u ia_trekking.service -f`
- Habilitar no boot: `sudo systemctl enable ia_trekking.service`

## Acesso e transferência de arquivos

- Acesso SSH ao Arduino: `ssh arduino@192.168.1.50`
- Transferir arquivo do PC para o Arduino:
  - `scp /caminho/ia_trekking.py arduino@ip:/caminho/`
- Transferir arquivo do Arduino para o PC:
  - `scp arduino@ip-do-robo:/caminho/arquivo.txt /caminho/`

## Outros comandos úteis

- Listar portas seriais disponíveis: `ls /dev/tty*`
- Verificar uso de CPU: `top` ou `htop`
