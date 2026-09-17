#!/usr/bin/env python3
"""
Grava em .txt as distancias dos ultrassonicos emitidas pelo MCU.

O MCU imprime, a cada INTERVALO_DIST_MS, uma linha de formato fixo:

    DIST;<millis>;<frente>;<direita>;<esquerda>

Este script le essas linhas da entrada padrao, ignora todo o resto da
telemetria e grava um arquivo com hora de parede na frente:

    2026-09-17 09:14:22.481;12345;40.2;999.0;35.1

Uso (na placa):

    arduino-cli monitor -p internal --fqbn arduino:zephyr:unoq \
        | python3 log_ultrassom.py distancias.txt

Le de stdin de proposito: assim nao depende de descobrir o caminho do
/dev/tty do MCU, e da para testar alimentando linhas na mao.
"""

import sys
from datetime import datetime

PREFIXO = "DIST;"
CABECALHO = "# hora;millis;frente_cm;direita_cm;esquerda_cm\n"


def main():
    saida = sys.argv[1] if len(sys.argv) > 1 else "distancias.txt"

    # line buffering: o arquivo fica legivel enquanto o robo anda, sem
    # precisar encerrar o processo para o conteudo aparecer.
    with open(saida, "a", buffering=1, encoding="utf-8") as f:
        if f.tell() == 0:
            f.write(CABECALHO)

        gravadas = 0
        for linha in sys.stdin:
            linha = linha.strip()
            if not linha.startswith(PREFIXO):
                continue

            campos = linha[len(PREFIXO):].split(";")
            if len(campos) != 4:          # linha truncada pela serial
                continue

            hora = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            f.write(f"{hora};" + ";".join(campos) + "\n")

            gravadas += 1
            if gravadas % 50 == 0:
                print(f"{gravadas} leituras em {saida}", file=sys.stderr)

    print(f"Fim. {gravadas} leituras gravadas em {saida}", file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
