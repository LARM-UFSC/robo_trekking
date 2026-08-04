import cv2
import time
import os
import sys

sys.path.insert(0, '/home/arduino/.local/lib/python3.13/site-packages')
sys.path.insert(0, '/usr/local/lib/python3.13/dist-packages')

from ultralytics import YOLO
from arduino.app_utils import Bridge 

MODEL_PATH = 'best_ncnn_model'
frame_count = 0
start_time = time.time()

try:
    model = YOLO(MODEL_PATH, task='segment')
    print("YOLO carregado")
except Exception as e:
    print(f"Erro ao carregar o modelo: {e}")
    sys.exit()

cap = cv2.VideoCapture(2)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

def loop():
    global frame_count, start_time
    
    if not cap.isOpened():
        return

    ret, frame = cap.read()
    if not ret:
        return

    results = model(frame, conf=0.6, imgsz=320, stream=True, verbose=False)
    comando = "S"

    for r in results:
        if r.boxes:
            box = r.boxes[0]
            nome = model.names[int(box.cls[0])]
            
            if nome == 'cone':
                x1, _, x2, _ = box.xyxy[0].cpu().numpy()
                cx = int((x1 + x2) / 2)
                
                if cx < 260:
                    comando = "L"
                elif cx > 380:
                    comando = "R"
                else:
                    comando = "F"
                break 

    try:
        Bridge.call("processa_direcao", comando)
    except Exception as e:
        print(f"Erro na chamada da Bridge: {e}")

    frame_count += 1
    if frame_count >= 30:
        end_time = time.time()
        fps = 30 / (end_time - start_time)
        print(f" Comando enviado: {comando} | FPS: {fps:.2f} ---")
        frame_count = 0
        start_time = time.time()

if __name__ == "__main__":
    try:
        while True:
            loop()
            time.sleep(0.01)
    except KeyboardInterrupt:
        try:
            Bridge.call("processa_direcao", "S")
        except:
            pass
        cap.release()
