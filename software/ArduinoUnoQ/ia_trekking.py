# pendencias:
#


import cv2
import time
import os
import sys
import glob
import argparse #visual, versao python de argc argv

from ultralytics import YOLO
from arduino.router_bridge import Bridge 
from pathlib import Path
from datetime import datetime

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

ARQUIVO_DIST   = str(BASE_DIR / "distancias.txt")
CABECALHO_DIST = "# hora;millis;frente_cm;direita_cm;esquerda_cm\n"
ultimas_distancias = {"frente": None, "direita": None, "esquerda": None}
_arq_dist = None
contornando = False

#=======================================================================
def registra_distancias(millis, frente, direita, esquerda):
    """Chamado pelo MCU. Uma linha por medicao do ultrassom."""
    global _arq_dist, ultimas_distancias

    ultimas_distancias["frente"] = frente
    ultimas_distancias["direita"] = direita
    ultimas_distancias["esquerda"] = esquerda

    if _arq_dist is None:
        # buffering=1: da para acompanhar o arquivo com o robo andando
        _arq_dist = open(ARQUIVO_DIST, "a", buffering=1, encoding="utf-8")
        if _arq_dist.tell() == 0:
            _arq_dist.write(CABECALHO_DIST)

    hora = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    _arq_dist.write(f"{hora};{millis};{frente:.1f};{direita:.1f};{esquerda:.1f}\n")

try:
    bridge.provide("registra_distancias", registra_distancias)
    print(f"Log de distancias ativo em {ARQUIVO_DIST}")
except Exception as e:
    print(f"Sem log de distancias pela bridge: {e}")

CAM_GLOB = '/dev/v4l/by-id/*046d_0825*index0'

#==================================================================
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

#================================================

def Vendo(results):
    for r in results:
        if r.boxes:
            for box in r.boxes:
                nome = model.names[int(box.cls[0])]

                if nome == "cone":
                    x1, y1, x2, y2 = box.xyxy[0].cpu().numpy()
                    return y1, y2

    return None

#==============================================

def CalcularDirecao(cx):
    gradiente = int((cx - 320) * 255 / 320)

    return max(-255, min(255, gradiente))   

#===============================================

def Perto(y1, y2):
    alturaBbox =  y2 - y1

    if alturaBbox >= 60:
        return True
    
    return False

#================================================

def DecidirModo(results, contornando):
    dados = Vendo(results)
    
    if dados is not None:
        y1, y2 = dados
        
        if Perto(y1, y2):
            contornando = True
            return "contornar", contornando
        else:
            contornando = False
            return "aproximar", contornando
    else:
        if contornando:
            return "contornar", contornando
        else:
            return "procurar", contornando
      

#====================================loop=====================================#
def loop():
    global frame_count, start_time, cap, falhas, contornando

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
    gradiente = 0
    encontrou = False
    modo, contornando = DecidirModo(results, contornando)

    for r in results:
       if r.boxes:
            for box in r.boxes:
                nome = model.names[int(box.cls[0])]

                if nome == "cone":
                    x1, y1, x2, y2 = box.xyxy[0].cpu().numpy()

                    cx = (x1 + x2) / 2

                    gradiente = CalcularDirecao(cx)
                    encontrou = True
                    break
            if encontrou: #so pra corrigir o caso especifico de ser 0 no gradiente
                break

    try:
        bridge.call("processa_direcao", gradiente, modo)
    except Exception as e:
        print(f"Erro na chamada da Bridge: {e}")    

    if VISUAL:
        AtualizarFrame(results, gradiente, modo)   

    frame_count += 1
    
    if frame_count >= 30:
        end_time = time.time()
        fps = 30 / (end_time - start_time)
        print(f" Gradiente enviado: {gradiente} | Modo: {modo} | FPS: {fps:.2f} ---")
        frame_count = 0
        start_time = time.time()
#====================================||==========================================#


#==========parte visual==========#


parser = argparse.ArgumentParser()
parser.add_argument('--visual', action='store_true', help='pra ver no navegador o que a camera ta vendo + printar distancias dos ultrassons')
args = parser.parse_args()
VISUAL = args.visual

if VISUAL:
    from http.server import BaseHTTPRequestHandler, HTTPServer
    import threading

    frame_visual = None
    lock_visual = threading.Lock()

    class VisualHandler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == '/video':
                self.send_response(200)
                self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=frame')
                self.end_headers()
                while True:
                    with lock_visual:
                        atual = frame_visual
                    if atual is None:
                        time.sleep(0.01)
                        continue
                    self.wfile.write(b'--frame\r\n')
                    self.wfile.write(b'Content-Type: image/jpeg\r\n')
                    self.wfile.write(f'Content-Length: {len(atual)}\r\n\r\n'.encode())
                    self.wfile.write(atual)
                    self.wfile.write(b'\r\n')
                    time.sleep(0.03)

    def iniciar_servidor_visual():
        HTTPServer(('0.0.0.0', 8000), VisualHandler).serve_forever()

    threading.Thread(target=iniciar_servidor_visual, daemon=True).start()
    print("modo visual ativado: http://192.168.1.194:8000/video")


def AtualizarFrame(results, gradiente, modo):
    global frame_visual
    anotado = results[0].plot()

    d = ultimas_distancias
    if d["frente"] is not None:
        texto_dist = f"F:{d['frente']:.0f}cm D:{d['direita']:.0f}cm E:{d['esquerda']:.0f}cm"
    else:
        texto_dist = "aguardando distancias"

    cv2.putText(anotado, texto_dist, (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

    cv2.putText(anotado, f"Gradiente: {gradiente} | Modo: {modo}", (10, 60),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

    ok, buffer = cv2.imencode('.jpg', anotado)
    if ok:
        with lock_visual:
            frame_visual = buffer.tobytes()
#===============fim opcao visual=================#


if __name__ == "__main__":
    try:
        while True:
            loop()
            time.sleep(0.01)
    except KeyboardInterrupt:
        try:
            bridge.call("processa_direcao", 0, "procurar")
        except:
            pass
        if cap is not None:
            cap.release()
        if _arq_dist is not None:
            _arq_dist.close()