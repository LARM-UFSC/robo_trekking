import os
from ultralytics import YOLO


BASE = './dataset'
yaml_path = os.path.join(BASE, 'data.yaml')
runs_dir = os.path.join(BASE, 'runs')

if not os.path.exists(yaml_path):
    print(f"Erro: O arquivo {yaml_path} não foi encontrado.")
    exit()

print("Treinamento YOLO (detecção) ")
# carrega a arquitetura base de deteccao (nano)
model = YOLO('yolo26n.pt')

model.train(
    data=yaml_path,
    epochs=100,
    imgsz=320,          # resolucao baixa para inferencia rapida
    batch=32,
    patience=20,
    device=0,           # usa a placa de video local
    project=runs_dir,
    name='trekking_ncnn',
    save=True
)
print("Treinamento Concluído!")

print("Exportação para NCNN")

# localiza os melhores pesos que acabaram de ser treinados
best_weights = os.path.join(runs_dir, 'trekking_ncnn/weights/best.pt')

if not os.path.exists(best_weights):
    print(f"Erro: Arquivo {best_weights} não encontrado. O treino falhou?")
    exit()

# carrega os pesos finais e faz a exportacao
model_trained = YOLO(best_weights)

# adicionar task segment caso for de futuro interesse. por enquanto, apenas detect
model_trained.export(format='ncnn', imgsz=320)

print("A pasta NCNN foi gerada e esta pronta para ir pro Arduino Uno Q.")