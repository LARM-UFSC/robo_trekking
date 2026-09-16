import cv2
import time
import os
import sys
import glob

from ultralytics import YOLO
from arduino.router_bridge import Bridge 
from pathlib import Path

bridge = Bridge()
bridge.connect(timeout=5)

BASE_DIR = Path(__file__).resolve().parent
MODEL_PATH = str(BASE_DIR.parent / "VisãoComputacional" / "best_ncnn_model")

frame_count = 0
start_time = time.time()

try:
    model = YOLO(MODEL_PATH, task='detect')
    print("YOLO carregado")
except Exception as e:
    print(f"Erro ao carregar o modelo: {e}")
    sys.exit()

CAM_GLOB = '/dev/v4l/by-id/*046d_0825*index0'

def abrirCamera():
    caminhos = sorted(glob.glob(CAM_GLOB))
    if not caminhos:
        return None
    caminho_real = os.path.realpath(caminhos[0])
    try:
        indice = int(caminho_real.replace('/dev/video', ''))
    except ValueError:
        print(f"Não consegui extrair índice de {caminho_real}")
        return None
    c = cv2.VideoCapture(indice, cv2.CAP_V4L2)
    if not c.isOpened():
        c.release()
        return None
    c.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    c.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    c.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    print(f"Camera aberta em /dev/video{indice} (symlink: {caminhos[0]})")
    return c


# a camera entrega 30 FPS e a inferencia consome ~6. O V4L2 enfileira o excesso
#e cap.read() devolve o ultimo frame  da fila entao o robo decide sobre uma
#imagem de varias centenas de ms atras.
# aqui descartamos a fila para ficar com o frame mais novo. O criterio e o tempo
# do proprio grab: frame que ja estava na fila volta na hora, enquanto o primeiro
# grab que precisa ESPERAR o sensor indica que chegamos na borda viva. Assim o
# descarte se auto-ajusta e nao trava se a fila for curta ou o FPS cair.
LIMITE_GRAB_MS = 5.0
MAX_DESCARTES  = 8

def frameMaisRecente(c):
    for _ in range(MAX_DESCARTES):
        inicio = time.time()
        if not c.grab():
            return False, None
        if (time.time() - inicio) * 1000.0 > LIMITE_GRAB_MS:
            break
    return c.retrieve()

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

    ret, frame = frameMaisRecente(cap)
    if not ret:
        falhas += 1
        if falhas >= 10:
            print("Camera caiu, reabrindo...")
            cap.release()
            cap = None
            falhas = 0
        return

    falhas = 0

    results = model(frame, conf=0.8, imgsz=320, verbose=False)
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
        bridge.call("processa_direcao", comando)
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
            bridge.call("processa_direcao", "S")
        except:
            pass
        if cap is not None:
            cap.release()
