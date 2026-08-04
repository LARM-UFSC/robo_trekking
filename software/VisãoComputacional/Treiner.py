import os
from ultralytics import YOLO

# Ajuste para a pasta onde está o seu dataset e o arquivo data.yaml
BASE = './dataset'
yaml_path = os.path.join(BASE, 'data.yaml')
runs_dir = os.path.join(BASE, 'runs')

if not os.path.exists(yaml_path):
    print(f"Erro: O arquivo {yaml_path} não foi encontrado.")
    exit()

print("Treinamento YOLO-Seg ")
# Carrega a arquitetura base de segmentação (Nano)
model = YOLO('yolov26n-seg.pt')

model.train(
    data=yaml_path,
    epochs=100,
    imgsz=320,          # Resolução baixa para inferencia rapida
    batch=32,
    patience=20,
    device=0,           # Usa a placa de vídeo local (mude para 'cpu' se der erro)
    project=runs_dir,
    name='trekking_ncnn',
    save=True
)
print("Treinamento Concluído!")

print("Exportação para NCNN")

# Localiza os melhores pesos que acabaram de ser treinados
best_weights = os.path.join(runs_dir, 'trekking_ncnn/weights/best.pt')

if not os.path.exists(best_weights):
    print(f"Erro: Arquivo {best_weights} não encontrado. O treino falhou?")
    exit()

# Carrega os pesos finais e faz a exportação
model_trained = YOLO(best_weights)

# task='segment' é crucial para manter a capacidade de gerar os polígonos
model_trained.export(format='ncnn', imgsz=320, task='segment')

print("A pasta NCNN foi gerada e está pronta para ir pro Arduino Uno Q.")