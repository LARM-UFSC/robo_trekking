# firmware_unoq — firmware do MCU do Arduino Uno Q

Controle do robô de trekking: recebe a posição do cone e o modo de operação da
CPU Linux pela Bridge, lê três ultrassônicos, e aciona os dois motores por meio
de um controlador fuzzy.

---

## Arquitetura

O Arduino Uno Q tem dois processadores no mesmo módulo, e o robô usa os dois:

| lado | o que roda | responsabilidade |
|---|---|---|
| **Linux** (Qualcomm) | `ia_trekking.py` + YOLO/NCNN | enxergar o cone, decidir o modo |
| **MCU** (STM32U585 / Zephyr) | este firmware | ler os sensores, decidir o movimento, acionar os motores |

A comunicação é RPC pela `Arduino_RouterBridge`. A CPU chama
`processa_direcao(gradiente, modo)` no MCU; o MCU não chama nada de volta.

A divisão de responsabilidade é deliberada: **a CPU manda a intenção, o MCU a
realiza usando os sensores locais.** A visão não vê obstáculos; o ultrassom não
reconhece cone.

---

## Hardware

Dois motores, em duas pontes H de um módulo L298N duplo:

- **Tração** — frente/trás. É o que desloca o robô.
- **Direção** — esterço com mola de centragem. Solto, volta ao centro sozinho,
  então o duty funciona como comando de posição: ele define o equilíbrio entre
  o torque do motor e a mola.

A direção é do tipo Ackermann. **O robô não gira no próprio eixo** — toda
mudança de orientação exige deslocamento.

### Pinos

| função | pino | porta |
|---|---|---|
| tração frente | 9 | PB8, TIM4_CH3 |
| tração trás | 10 | PB9, TIM4_CH4 |
| direção esquerda | 3 | PB0, TIM3_CH3 |
| direção direita | 11 | PB15, TIM1_CH3N |
| TRIGGER ultrassom (comum aos 3) | 2 | PB3 |
| ECHO_1 — frente | 4 | PA12 |
| ECHO_2 — direita | 6 | PB1 |
| ECHO_3 — esquerda | 8 | PB4 |
| ECHO_4 — traseiro, reservado | 13 | PB13 |
| I2C do MPU6050 | 20 / 21 | PB11 / PB10 |

Os sensores HC-SR04 estão alimentados a **3,3 V**, fora da especificação de 5 V
do módulo. Ver "Características medidas".

### Convenções de sinal

Duas convenções opostas convivem no sistema, e confundi-las é o erro mais
provável de todos:

| grandeza | positivo significa |
|---|---|
| `gradienteCone` (vem da câmera) | cone à **direita** |
| saída de direção (vai ao motor) | esterço à **esquerda** |

A inversão entre as duas está feita **nas regras fuzzy** — `coneDir` aponta para
`dirSuave` — e não em conta aritmética. Não inverta de novo no código.

---

## Protocolo da Bridge

```python
bridge.call("processa_direcao", gradiente, modo)
```

**`gradiente`** (int, −255 a +255) — posição horizontal do cone no quadro,
calculada como `(cx − 320) × 255 / 320`. Zero é centralizado, +255 é a borda
direita. Em `procurar` a CPU envia 0.

**`modo`** (string) — o MCU guarda só a inicial:

| modo | quando | origem |
|---|---|---|
| `aproximar` | cone visível e longe | altura da bbox < 60 px |
| `contornar` | cone visível e perto | altura da bbox ≥ 60 px |
| `procurar` | sem cone à vista | nenhuma detecção |

---

## Fluxo de controle

### Camada de cima — despacho por modo

O `.ino` não implementa comportamento. Ele recebe o comando, traduz em modo,
despacha para o serviço correspondente, aplica as travas e aciona:

```cpp
switch (modoAtual()) {
  case 'p': passoBusca(vel, dir);                    break;
  case 'a': passoFuzzy(gradienteCone, vel, dir);     break;
  case 'c': passoContorno(gradienteCone, vel, dir);  break;
  default:  /* 'x' = sem link, fica em zero */       break;
}
aplicarTravas(vel, dir);
```

| modo | serviço | estado |
|---|---|---|
| `'p'` procurar | `passoBusca` — círculo com ré de contra-esterço | pronto |
| `'a'` aproximar | `passoFuzzy` — segue o cone e desvia | pronto |
| `'c'` contornar | `passoContorno` — orbita o cone | **esqueleto** |
| `'x'` sem link | nada, `vel = dir = 0` | pronto |

Todos os serviços têm a mesma forma: entradas por parâmetro, `vel` e `dir` por
referência. **Nenhum deles sabe em que modo foi chamado** — sabe apenas executar
o próprio comportamento. O vocabulário de modos existe só no `.ino`.

O modo é a inicial do que a CPU manda, e link caído vira `'x'` — mesma convenção
que o `desenharLed.hpp` já usava. Não há `enum`: o `.ino` gera protótipos
automáticos **antes** das suas próprias definições, então função que devolve tipo
declarado no `.ino` não compila. O `char` resolve sem precisar de um header só
para o vocabulário.

O failsafe de link serve também como **botão de emergência**: matar o
`ia_trekking.py` para o robô em menos de um segundo.

#### Busca em círculo, com ré de contra-esterço

| sub-estado | tração | direção |
|---|---|---|
| avançando | `+VEL_BUSCA` | `+DIR_BUSCA` (esquerda) |
| recuando | `-dutyRe` | `-DIR_BUSCA` (direita) |

O contra-esterço na ré não é detalhe de ajuste, é geometria. Avançar esterçado à
esquerda gira o robô no sentido anti-horário; recuar com o **mesmo** esterço
percorre o arco de volta e gira no sentido oposto, desfazendo exatamente a
rotação que o avanço produziu — o robô oscilaria entre dois pontos varrendo
sempre o mesmo pedaço do horizonte. Com o esterço invertido, os dois segmentos
somam rotação no mesmo sentido. É a manobra de três tempos.

Entra em ré quando a frente **ou o lateral do lado para onde gira** fica abaixo
de `LIMIAR_BUSCA_BLOQUEIO`. Girando à esquerda, parede à esquerda impede a curva
mesmo com a frente livre.

Sai da ré quando `dist_1 > LIMIAR_BUSCA_LIVRE`, ou pelo teto de tempo. A saída
é **medida**, não cronometrada: recuando de uma parede, a distância frontal
cresce. Os dois limiares diferentes dão histerese.

**A ré é cega.** O sensor traseiro não está ligado, então `TETO_RE_MS` limita o
estrago mas não evita colisão por trás. Quando o `ECHO_4` entrar, a condição de
saída da ré passa a ser ele.

### Camada de baixo — controlador fuzzy (eFLL 1.5.1)

Quatro entradas, duas saídas, 15 regras. Mamdani com MAX-MIN, defuzzificação por
centro de área.

A base de regras é montada **uma única vez** em `inicializarFuzzy()`, chamada do
`setup()`. A eFLL aloca com `new`; construir regra dentro do `loop()` vazaria
memória até derrubar a placa.

---

## Tabela de regras

Direção positiva é esquerda.

### Desvio — 12 regras pelas distâncias

| # | frente | direita | esquerda | direção | velocidade |
|---|---|---|---|---|---|
| 1 | LONGE | LONGE | LONGE | — | RAPIDA |
| 2 | LONGE | LONGE | PERTO | DIR_SUAVE | RAPIDA |
| 3 | LONGE | PERTO | LONGE | ESQ_SUAVE | RAPIDA |
| 4 | LONGE | PERTO | PERTO | CENTRO | MEDIA |
| 5 | MEDIO | LONGE | LONGE | CENTRO | MEDIA |
| 6 | MEDIO | LONGE | PERTO | DIR_SUAVE | MEDIA |
| 7 | MEDIO | PERTO | LONGE | ESQ_SUAVE | MEDIA |
| 8 | MEDIO | PERTO | PERTO | CENTRO | LENTA |
| 9 | PERTO | LONGE | LONGE | ESQ_FORTE | LENTA |
| 10 | PERTO | LONGE | PERTO | DIR_FORTE | LENTA |
| 11 | PERTO | PERTO | LONGE | ESQ_FORTE | LENTA |
| 12 | PERTO | PERTO | PERTO | CENTRO | LENTA |

A **regra 1** não opina sobre direção (`addRegraVel`). Se ela mandasse `CENTRO`,
puxaria o esterço para zero e brigaria com o seguimento do cone o tempo todo.

A **regra 9** é o empate: frente bloqueada com os dois lados livres não tem lado
melhor. `ESQ_FORTE` é escolha arbitrária, e o que importa é ser sempre a mesma —
um robô que contorna sempre pelo mesmo lado é previsível.

A **regra 12** é o cercado. Ver pendência 1.

### Seguimento do cone — 3 regras

| # | frente | cone | direção |
|---|---|---|---|
| 13 | LONGE | ESQUERDA | ESQ_SUAVE |
| 14 | LONGE | CENTRO | CENTRO |
| 15 | LONGE | DIREITA | DIR_SUAVE |

Mandam **só direção**, nunca velocidade. O `longeFr` no antecedente é proteção:
com obstáculo a menos de ~80 cm essas três param de disparar e a direção passa a
ser decidida pelas regras 5–12, que usam os conjuntos FORTE e dominam a
composição. **Emergência ganha de perseguição, por construção.**

### Conjuntos

**Distâncias (cm)**

| entrada | PERTO | MEDIO | LONGE |
|---|---|---|---|
| frente | 0–45 | 30–110 | 80–400 |
| laterais | 0–80 | — | 30–400 |

Os laterais têm só dois conjuntos, para manter a base em 12 combinações em vez
de 27. A sobreposição ampla (30–80) é intencional: sem ela haveria faixa de
distância sem nenhuma regra ativa, e saída indefinida.

**Cone (gradiente)** — ESQUERDA `−255…−20`, CENTRO `−60…+60`, DIREITA `+20…+255`

**Velocidade** — LENTA `0–170`, MEDIA `140–220`, RAPIDA `190–255`

**Direção** — DIR_FORTE `−255…−130`, DIR_SUAVE `−200…−90`, CENTRO `−70…+70`,
ESQ_SUAVE `+90…+200`, ESQ_FORTE `+130…+255`

Os conjuntos das pontas são ombros ancorados em ±255, não triângulos. Com
triângulo, o centroide nunca alcança o extremo e **esterço no batente seria
inatingível** mesmo com só aquela regra disparando.

O intervalo entre `CENTRO` e os `SUAVE` (70 a 90) é a zona morta do esterço:
abaixo de certo duty o motor não vence a mola e só gera corrente.

Como os conjuntos se sobrepõem, várias regras disparam simultaneamente e o
centro de área mistura as saídas. A tabela descreve os cantos do espaço; o
comportamento real interpola entre as linhas.

---

## Características medidas

### Sensores ultrassônicos

Caracterizados sobre 39.360 amostras reais, em 2026-09-18.

Perda de eco por faixa de distância:

| faixa | frente | direita | esquerda |
|---|---|---|---|
| 0–50 cm | 0,8% | 6,2% | 14,6% |
| 50–100 cm | 3,7% | 0,9% | 1,9% |
| 100–150 cm | 14,7% | 1,4% | 6,8% |
| 150–200 cm | 20,7% | 1,0% | 2,7% |
| 250–300 cm | 2,0% | — | 27,5% |
| 300–400 cm | 41,2% | 25,0% | 0,3% |

A tabela parece contraditória até se notar o que ela diz: **a perda depende mais
do tipo de alvo que da distância.** Parede lisa e perpendicular devolve eco a
3,5 m sem esforço; objeto irregular ou angulado se perde depois de ~1 m.

**Faixa de trabalho confiável: 20 a 150 cm.** Abaixo de 20 cm a perda sobe nos
laterais; acima de 150 cm só funciona com superfície grande e perpendicular.

Precisão dentro da faixa é excelente: 48 minutos contínuos com alvo a 43 cm
deram 0,0% de perda e dispersão de 0,4 cm.

**Leitura inválida vale 999 e é tratada como "livre"** (entra no fuzzy como 400).
É decisão consciente: o HC-SR04 perde eco em superfície angulada o tempo todo, e
tratar isso como bloqueio pararia o robô à toa. A consequência é que sensor com
defeito falha para o lado de **seguir andando**.

### Taxa de atualização

| caminho | período |
|---|---|
| visão (câmera → Bridge) | ~200 ms (≈5 FPS, medido em 2026-09-25) |
| ultrassom | 60 ms |
| `loop()` do MCU | ~80 ms |

A visão é o elo mais lento. A 5 FPS, o comando de cone que o MCU executa tem até
200 ms de idade por natureza — o failsafe não é o gargalo de latência, a câmera é.

---

## Build e upload

```bash
arduino-cli compile --fqbn arduino:zephyr:unoq firmware_unoq
```

```bash
arduino-cli upload -p internal --fqbn arduino:zephyr:unoq firmware_unoq
```

O `-p internal` grava o MCU interno a partir do lado Linux da própria placa. A
biblioteca **eFLL** precisa estar instalada no `arduino-cli` da máquina que
compila — a eFLL é compilada **dentro** do binário, nada é instalado no MCU.

Dependências: `Arduino_RouterBridge`, `Arduino_LED_Matrix`, `eFLL 1.5.1`, e
`Adafruit MPU6050` apenas se `USAR_MPU` for 1.

Consumo atual: 93.388 bytes de flash (11%), 35.706 de RAM (13%).

---

## Organização dos arquivos

| arquivo | serve | estado |
|---|---|---|
| `firmware_unoq.ino` | Bridge, tradução do modo, despacho, acionamento | — |
| `motores.hpp` | `acionarPonte`, `aplicarEsterco`, `pararMotores` | pronto |
| `ultrassom.hpp` | `atualizarSonares`, `dist_1..3`, `houveMedicaoNova` | pronto |
| `desvioFuzzy.hpp` | `inicializarFuzzy`, `passoFuzzy` | pronto |
| `busca.hpp` | `passoBusca`, `reiniciarBusca` | pronto |
| `contorno.hpp` | `passoContorno`, `reiniciarContorno` | **esqueleto** |
| `travas.hpp` | `aplicarTravas` | **esqueleto** |
| `desenharLed.hpp` | `inicializarLedModo`, `atualizarLedModo` | pronto |
| `mpu.hpp` | `inicializarMPU`, `lerMPU` — gated por `USAR_MPU` (hoje 0) | pronto |

Cada header é incluído **apenas** pelo `.ino`. Funções `inline` e estado
`static`, para não quebrar a linkagem. Headers que dependem de outros incluem
suas próprias dependências, então a ordem dos `#include` no `.ino` não importa.

**Duplicação consciente:** `busca.hpp` e `contorno.hpp` usam a mesma manobra de
círculo com ré de contra-esterço, e cada um tem a sua cópia. É escolha
deliberada, para os dois serem escritos em paralelo sem conflito. Enquanto durar,
consertar a ré é consertar nos **dois**.

---

## Constantes a calibrar

| constante | arquivo | valor | situação |
|---|---|---|---|
| `ESTERCO_MIN_UTIL` | `.ino` | 90 | zona morta do esterço, nunca medida |
| `VEL_BUSCA` | `.ino` | 170 | duty do giro de busca, nunca medido |
| `dutyRe` | `motores.hpp` | 140 | usado pela ré da busca; nunca foi acionado até agora |
| `LIMIAR_BUSCA_BLOQUEIO` | `.ino` | 40 cm | entra em ré |
| `LIMIAR_BUSCA_LIVRE` | `.ino` | 70 cm | volta a avançar |
| `TETO_RE_MS` | `.ino` | 1500 ms | teto da ré cega |
| `JANELA_CENTRADO` | `.ino` | 50 | ~63 px de 640; sem uso no momento |
| `dutyDirecao` | `motores.hpp` | 255 | marcado AJUSTAR desde o início |
| `TIMEOUT_COMANDO_MS` | `.ino` | 700 | 3,5× o período a 5 FPS — dentro da faixa, na ponta apertada |

`dutyDirecao = 255` é o máximo e provavelmente encosta o esterço no batente, o
que gera corrente sem deslocamento. É o que torna necessário o teto térmico
`ESTERCO_MAX_MS` (2500 ms ligado, 500 ms de alívio obrigatório).

A velocidade do robô nunca foi medida, então a **distância cega** — quanto ele
anda entre dois comandos da câmera — é desconhecida. A 1 m/s seriam 20 cm.

---

## Pendências

### 1. Nenhuma trava de parada

Nada zera a velocidade além da queda do link. No cercado (regra 12) a saída é
`LENTA`, cujo centroide fica por volta de 70, então o robô **rasteja contra o
obstáculo em vez de parar**. Sintoma já observado em teste.

As travas precisam ficar **fora do fuzzy**, como sobrescrita entre o
`defuzzify()` e o acionamento, num único ponto capaz de zerar os motores.
Situações a cobrir:

- **cercado** — três sensores abaixo do limiar
- **chegou no cone** — `modo == 'contornar'` **e** `dist_1` abaixo do limiar.
  A câmera sozinha erra por bbox alta de outra coisa; o ultrassom sozinho não
  distingue cone de parede. Juntos, cada um cobre o furo do outro.
- **obstáculo crítico frontal** — seguro barato, independente dos laterais

Duas armadilhas ao implementar:

**Debounce por medição, não por iteração.** O `loop()` roda a ~80 ms e o sonar
atualiza a cada 60 ms, mas um contador incrementado a cada volta satura sobre a
mesma leitura. Confirmar em N medições exige um marcador de medição nova no
`ultrassom.hpp`.

**Histerese.** Com limiar único, leitura oscilando em torno dele faz o robô
alternar entre parado e andando várias vezes por segundo. O limiar de soltar
deve ser maior que o de travar.

### 2. Comportamento incompleto

- **Sensor traseiro desligado.** A ré da busca é cega, limitada só por tempo.
  O pino 13 está reservado e o `ultrassom.hpp` tem a varredura escrita para N
  sensores — falta ligar e descomentar.
- **Contorno do cone não implementado.** O `modo == 'contornar'` chega ao MCU e
  não controla nada.
- **`manobra.hpp` vazio.** Contorno e retorno a escrever — ou a descartar, se a
  lógica ficar do lado Python.
- **MPU desligado.** Inclinação sem uso; não há proteção anti-tombamento.

### 3. Sem diagnóstico do fuzzy

`imprimirTelemetria()` foi removido, então `dir` e `vel` — as duas saídas do
controlador — não aparecem em lugar nenhum. Sobram o overlay do vídeo
(gradiente e modo) e a matriz de LED (modo), ambos do lado da câmera.

Quando precisar depurar uma decisão do fuzzy, o caminho é uma impressão
temporária junto de `fuzzy->isFiredRule(i)`, que mostra quais regras dispararam
no ciclo. É o antídoto para a maior desvantagem do fuzzy, que é não saber por que
ele decidiu o que decidiu.

### 4. Lacunas no lado Linux, sem causa determinada

Durante os testes de setembro o log parou por intervalos de 16 a 276 segundos,
sete vezes. O `millis` do MCU e o relógio do Linux avançaram igual nas lacunas,
o que prova que **o MCU rodou o tempo todo** — quem sumiu foi o processo Python.

Enquanto ele está fora, ninguém chama `processa_direcao`, o failsafe dispara, e o
robô fica parado pela lacuna inteira.

A causa nunca foi determinada, e o instrumento que as detectava (gravação em
`distancias.txt`) foi removido. Com a visão rodando como serviço, o journal
distingue as duas hipóteses: se `YOLO carregado` aparecer no meio da lacuna, o
processo morreu e voltou; se o log simplesmente retomar, ele travou vivo.

---

## Histórico

Em ordem aproximada:

1. **Failsafe de timeout** no comando da câmera. Antes, o último comando
   congelava e o robô seguia andando indefinidamente quando o link caía.
2. **Descarte de fila do V4L2** no lado Python. A câmera produz a 30 FPS e a
   inferência consome a ~5; `cap.read()` devolvia o frame mais antigo da fila,
   e o robô decidia sobre imagem de centenas de ms atrás.
3. **Modelo trocado de segmentação para detecção.** O código só usava caixa, e
   pagava pela máscara a cada frame.
4. **Caracterização dos sensores** com 39 mil amostras, que embasou as faixas
   dos conjuntos fuzzy.
5. **Desvio fuzzy** (eFLL) substituindo a máquina de estados discreta.
6. **Protocolo trocado** de letra única (`F`/`L`/`R`/`S`) para gradiente
   contínuo + modo, o que deu resolução real à entrada do controlador.
7. **Busca em círculo** no modo `procurar`.
8. **Indicação do modo na matriz de LED.**
9. **Limpeza**: removidos o caminho discreto inteiro, o retorno em U por tempo,
   o log de distâncias, a telemetria serial e as flags de compilação que
   sobraram.
