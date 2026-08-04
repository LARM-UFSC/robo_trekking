import os
import json
import cv2
from ultralytics import YOLO


# Defina o caminho absoluto ou relativo de onde estão as imagens e o JSON
# Exemplo: BASE = '/home/gabriel/projetos/Trekking/dataset'
BASE = './dataset' 

# Limpeza e preparação da pasta de labels
train_lab_dir = os.path.join(BASE, 'labels/train')
os.makedirs(train_lab_dir, exist_ok=True)

print("Limpando labels antigos...")
for f in os.listdir(train_lab_dir):
    if f.endswith('.txt'):
        os.remove(os.path.join(train_lab_dir, f))

# 2. CONVERSÃO COCO -> YOLO SEGMENTATION
json_path = os.path.join(BASE, '_annotations.coco.json')

try:
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
except FileNotFoundError:
    print(f"Erro: O arquivo {json_path} não foi encontrado.")
    exit()

img_map = {img['id']: img for img in data['images']}
print(f"Convertendo {len(data['annotations'])} anotações para YOLO Segmentation...")

for ann in data['annotations']:
    img_info = img_map[ann['image_id']]
    txt_filename = os.path.splitext(img_info['file_name'])[0] + ".txt"
    txt_path = os.path.join(train_lab_dir, txt_filename)

    yolo_class = ann['category_id'] - 1
    
    # Ignora classes negativas
    if yolo_class < 0: 
        continue

    w, h = img_info['width'], img_info['height']
    
    # Verifica se existe segmentação e pega o primeiro polígono
    if 'segmentation' in ann and len(ann['segmentation']) > 0:
        poly = ann['segmentation'][0]

        # Normaliza coordenadas (0.0 até 1.0)
        norm_poly = []
        for i in range(0, len(poly), 2):
            norm_poly.append(f"{poly[i]/w:.6f}")
            norm_poly.append(f"{poly[i+1]/h:.6f}")

        # Escreve no formato: <class> <x1> <y1> <x2> <y2> ...
        with open(txt_path, 'a') as f_out:
            f_out.write(f"{yolo_class} " + " ".join(norm_poly) + "\n")

print("Conversão concluída com sucesso!")