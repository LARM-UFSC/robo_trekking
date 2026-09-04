import cv2
import time
import os
import sys
import glob

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

CAM_GLOB = '/dev/v4l/by-id/*046d_0825*index0'

def abrirCamera():
    caminhos = sorted(glob.glob(CAM_GLOB))
    if not caminhos:
        return None
    c = cv2.VideoCapture(caminhos[0], cv2.CAP_V4L2)
    if not c.isOpened():
        c.release()
        return None
    c.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    c.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    print(f"Camera aberta em {caminhos[0]}")
    return c

cap = abrirCamera()
falhas = 0

def loop():
    global frame_count, start_time, cap, falhas

    if cap is None:
        cap = abrirCamera()
        if cap is None:
            time.sleep(1.0)
            return
        falhas = 0

    ret, frame = cap.read()
    if not ret:
        falhas += 1
        if falhas >= 10:
            print("Camera caiu, reabrindo...")
            cap.release()
            cap = None
            falhas = 0
        return

    falhas = 0

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
        if cap is not None:
            cap.release()
