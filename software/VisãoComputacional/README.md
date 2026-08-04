# Módulo de Visão Computacional do Trekking

Este README descreve o fluxo de treinamento e exportação do modelo de visão computacional utilizado no robô Trekking. O objetivo principal é transformar imagens capturada em um modelo de segmentação capaz de identificar objetos relevantes, como cones, obstáculos e a pista, e gerar um modelo pronto para inferência em hardware embarcado.

## Objetivo do módulo

O módulo de visão computacional usa uma abordagem baseada em detecção e segmentação por instâncias com YOLO, para gerar um modelo que pode ser exportado para o formato NCNN e utilizado em ambientes com menor capacidade computacional, como o Arduino Uno Q ou outro sistema embarcado Linux.

## Fluxo geral

```text
Coleta de imagens -> Anotação no Roboflow -> Exportação do dataset
-> Conversão de anotações para formato YOLO -> Treinamento do modelo
-> Exportação para NCNN -> Uso no sistema embarcado
```

## Estrutura da pasta

A pasta VisãoComputacional contém os arquivos principais abaixo:

```text
VisãoComputacional/
├── Conversao.py
├── Treiner.py
└── README.md
```

Os scripts esperam que o dataset seja organizado em uma pasta chamada `dataset`, localizada no mesmo diretório onde os scripts serão executados.

## Pré-requisitos

Antes de começar, instale as dependências necessárias:

```bash
pip install ultralytics opencv-python
```

Se você tiver uma GPU NVIDIA, o treino pode rodar de forma mais rápida. Caso contrário, pode trocar o valor de `device` no script para `cpu`.

## Passo 1: Coleta de dados e anotação no Roboflow

O sistema começa na plataforma Roboflow:

1. Acesse https://roboflow.com/
2. Crie um projeto para o dataset do Trekking.
3. Faça o upload das imagens capturadas, preferencialmente em diferentes ângulos, distâncias, posições e condições de iluminação.
4. Crie as classes do problema. No projeto atual, as classes esperadas são:
   - Cone
   - Obstáculo
   - Pista
5. Anote as imagens usando segmentação por instâncias. O Roboflow facilita esse processo e, se disponível, pode usar recursos como SAM3 para acelerar a anotação.
6. Exporte o dataset no formato COCO JSON, que gera arquivos como:
   - `_annotations.coco.json`
   - `data.yaml`
   - pasta com as imagens anotadas

### Observação importante

O Roboflow as vezes permite exportar diretamente em formatos compatíveis com YOLO em alguns casos. Se isso for possível, você pode pular a etapa de conversão. No entanto, o script atual da pasta `VisãoComputacional` foi preparado para trabalhar com datasets exportados no formato COCO JSON.

## Passo 2: Preparar o dataset localmente

Crie uma pasta chamada `dataset` dentro da pasta `VisãoComputacional` e organize os arquivos assim:

```text
VisãoComputacional/
├── Conversao.py
├── Treiner.py
├── dataset/
│   ├── images/
│   ├── _annotations.coco.json
│   └── data.yaml
```

O arquivo `data.yaml` deve conter as informações das classes e os caminhos do dataset. O script de conversão usa esse dataset para gerar os arquivos de labels no formato YOLO.

## Passo 3: Converter as anotações COCO para YOLO

O script `Conversao.py` lê o arquivo `_annotations.coco.json` e gera arquivos `.txt` de labels no formato esperado pelo YOLO.

### Como executar

```bash
cd VisãoComputacional
python Conversao.py
```

### O que o script faz

- lê as anotações do COCO;
- cria a pasta `dataset/labels/train`;
- limpa labels antigos;
- converte cada anotação para o formato YOLO de segmentação;
- gera um arquivo `.txt` para cada imagem, com as coordenadas dos polígonos anotados.

Após a execução, a estrutura fica semelhante a:

```text
dataset/
├── images/
├── labels/
│   └── train/
│       └── imagem1.txt
│       └── imagem2.txt
├── _annotations.coco.json
└── data.yaml
```

## Passo 4: Treinar o modelo

O script `Treiner.py` realiza o treinamento do modelo YOLO e já prepara a exportação para o formato NCNN.

### Como executar

```bash
cd VisãoComputacional
python Treiner.py
```

### O que o script faz

- carrega um modelo base de segmentação do Ultralytics;
- treina o modelo com o dataset localizado em `dataset`;
- salva os resultados em uma pasta de runs dentro do dataset;
- exporta o modelo final para o formato NCNN.

O treino usa a configuração atual do script com resolução `320x320`, o que é interessante para inferência mais rápida e leve.

## Passo 5: Exportação para NCNN e uso no embarcado

Ao final do treinamento, o modelo é exportado para o formato NCNN. Esse formato é indicado para execução em ambientes com recursos limitados.

O modelo gerado pode então ser copiado para o ambiente embarcado ou para o sistema Linux onde o robô vai executar a inferência.

## Dicas importantes

- Se o Roboflow exportar diretamente para YOLO, você pode dispensar o uso de `Conversao.py`.
- Se o treinamento falhar por falta de dependência, verifique a instalação do `ultralytics`.
- Se não houver GPU, troque `device=0` por `device='cpu'` no script `Treiner.py`.
- Mantenha as classes consistentes entre o Roboflow, o `data.yaml` e as anotações do dataset.

## Resumo

O fluxo completo do módulo é:

1. Capturar imagens;
2. Anotar no Roboflow;
3. Exportar o dataset;
4. Converter para YOLO se necessário;
5. Treinar o modelo;
6. Exportar para NCNN;
7. Utilizar o modelo no sistema embarcado.

