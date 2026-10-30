# visão computacional roboTrekking
> Documentação da parte de detecção de objetos embarcada

> Última atualização: 29/10/2026 por Arthur Fogliato

> Esta documentação descreve a organização dos arquivos, as tecnologias utilizadas, o processo de treinamento e exportação do modelo e sua integração ao software embarcado.

## 1. Introdução: Visão geral
Este projeto é baseado na categoria **Trekking** de robótica. Robôs deste modelo são sistemas autônomos projetados para navegar em determinado cenário com **autonomia completa**. O percurso geralmente exige que o robô saiba lidar com diferentes obstáculos, terrenos e situações. O objetivo é alcançar pontos de controle ou **objetos em específico**, e aqui entra a necessidade da **Visão Computacional Embarcada**.

No cenário em questão, o **Arduino Uno Q** possui um microprocessador(MPU) com ambiente Linux, capaz de executar aplicações em Python e rodar modelos treinados de Inteligência Artificial na borda. O MPU recebe imagens de uma câmera integrada à carenagem. O módulo de visão computacional é responsável por **processar esses dados e fornecer informações pertinentes à utilização no módulo de controle**, sendo orquestrado pelo arquivo _ia_trekking.py_. Dessa forma, as informações obtidas a partir das imagens podem ser utilizadas pela lógica de navegação, em conjunto com os demais recursos do sistema. 

O modelo utilizado foi treinado para reconhecer apenas a classe **cone** em potenciais cenários. As fotos foram selecinoadas visando detecção em imagens com drástica diferença de luz, movimentação e desfoque, além de angulações diferentes. Via Bridge, um _Remote Procedure Call_ interno do Arduino uno Q entre MPU e MCU, o modelo entrega:
 - Posição do cone na câmera em forma de gradiente horizontal: -255 (canto esquerdo), 0(centro) e 255(canto direito). 
 - Classificação estimada de **distância** conforme o tamanho da _bounding box_, crucial para _ia_trekking.py_ classificar entre modos Aproximar, Contornar ou Procurar.


## 2. Detalhes Técnicos

### 2.1. Hardware
|    item    |   especificação   |
|------------|-------------------|
|   Placa    |   Arduino UNO Q   |
|   MPU      |  Qualcomm QRB2210 |
|   MCU      |      STM32U58     |
|   Câmera   | Logitech C270 HD  |
|   HUB      | Adaptador USB-C   |

### 2.2. Software
|   item     |   versão       |
|------------|----------------|
|     OS     |                |
|   Python   |      3.13      |
|Ultralythics|                |
|   OpenCV   | python-headless|

### 2.3. Modelo
| Parâmetro |   especificação |
|-----------|-----------------|
|Arquitetura|YOLO26n(detecção)|
|  Formato  |      NCNN       |
|  Classes  |     1(cone)     |
| Resolução |     320x320     |
| Confiança |       0.8       |
|  Dataset  |   em  /dataset  |


> a câmera pode ser substituída por outras compatíveis com V4L2. o HUB é genérico.

> detalhes de outras partes do robô podem ser encontrados em outras partes do projeto

## 3. Termos Relacionados

### 3.1. Visão computacional
- YOLO: um algoritmo de detecção e classificação de objetos em imagens em tempo real;
- Detecção vs. segmentação: segmentação marca pixel a pixel do objeto. No projeto, utilizamos Deteccção: apenas a Bounding Box;
- Bounding box: retângulo que envolve o objeto detectado. "caixa" de coordenadas x1, x2, y1, y2;
- Classe: é possível criar e treinar para quantas classes forem desejadas. Aqui, foi escolhido apenas Cone;
- Inferência: é a utilização do modelo já treinado para o reconhecimento desejado. é a aplicação a cada frame da câmera;
- Confiança(`conf`): A certeza de que tal detecção é determinada classe. No projeto, abaixo de 0,8 é descartado;

### 3.2. Treinamento
- Dataset: conjunto de imagens coletadas e rotuladas;
- Anotação (rótulo / label): marcação manual, realizada por um humano em cada imagem;
- Fine-tuning (transfer learning): reaproveitar um modelo já treinado, ajustando-o para os objetos de interesse;
- Época: quantidade de tentativas de treinamento e refinamento até o resultado final;

### 3.3. Formatos e arquivos
- YAML: arquivo formato de texto, utilizado para configuração do treinamento;
- NCNN: um dos formatos de modelo final exportado pelo ultralythics. NCNN é leve, feito para rodar em CPUs de dispositivos embarcados;
- rotulação YOLO: formato .txt. ex: 0 0.871 0.395 0.238 0.671;
- rotulação COCO: formato JSON. ex: "image_id": 1, "category_id": 1, "bbox": [120, 80, 60, 150];

### 3.4. Ferramentas e bibliotecas
- Ultralytics: biblioteca Python que implementa o YOLO; treina, exporta e executa o modelo;
- OpenCV: biblioteca para lidar com imagens e vídeos. No projeto, registra as os frames da câmera;
- Roboflow: uma das plataformas online para anotar as imagens e organizar o dataset;

## 4. Estrutura de Pastas
    VisãoComputacional
        ├───best_ncnn_model
        └───dataset
            └───train
                ├───images
                └───labels
## 5. Treinamento


### 5. 1. Exportação do Modelo

## 6. Integração ao Robô
    //no ia_trekking tem a opcao da flag --visual pra acompanhar o que a camera ta vendo

### 6. 1. 



## 7. Referências
 - https://docs.arduino.cc/resources/datasheets/ABX00162-ABX00173-datasheet.pdf



//descrever onde usa o modelo no ia.python

