# visão computacional roboTrekking
> Documentação da parte de detecção de objetos embarcada

> Última atualização: 29/10/2026 por Arthur Fogliato

> Esta documentação descreve a organização dos arquivos, as tecnologias utilizadas, o processo de treinamento e exportação do modelo e sua integração ao software embarcado.

## 1. Introdução: Visão geral
Este projeto é baseado na categoria **Trekking** de robótica. Robôs deste modelo são sistemas autônomos projetados para navegar em determinado cenário com **autonomia completa**. O percurso geralmente exige que o robô saiba lidar com diferentes obstáculos, terrenos e situações. O objetivo é alcançar pontos de controle ou **objetos em específico**, e aqui entra a necessidade da Visão Computacional Embarcada.
No cenário em questão, o **Arduino Uno Q** possui um microprocessador(MPU) com ambiente Linux, capaz de executar aplicações em Python e rodar modelos treinados de Inteligência Artificial na borda. O MPU recebe imagens de uma câmera integrada à carenagem. O módulo de visão computacional é responsável por **processar esses dados e fornecer informações pertinentes à utilização no módulo de controle**, sendo orquestrado pelo arquivo _ia_trekking.py_. O modelo utilizado foi treinado para reconhecer apenas a classe **cone** e diferenciá-la de outros objetos em potenciais cenários. Dessa forma, as informações obtidas a partir das imagens podem ser utilizadas pela lógica de navegação, em conjunto com os demais recursos do sistema. 


## 2. Detalhes Tecnicos
//modelo certinho do MPU
//detalhes do linux usado, acho que é ubuntu mas VER DISTRO, informacoes do datasheet do arduino e da camera especifica dizer que vale pra outras cameras
//arduino uno Q com linux, modelo da camera, do hub, como liga tudo... *detalhes das outras partes do robo em outras partes do projeto

## 3. Termos Relacionados
//yolo, ultralythics, deteccao, yaml, ncnn, dataset, coco, treiner, detection, segmentation


## 4. Estrutura de Pastas
    //
## 5. Treinamento
### 5. 1. Exportação do Modelo

## 6. Integração ao Robô
### 6. 1. 



## 7. Referências








//detalhes tecnicos
//biblioteca utilizada. versao e parametros


//descrever onde usa o modelo no ia.python

