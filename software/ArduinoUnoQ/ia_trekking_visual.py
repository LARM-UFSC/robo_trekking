import cv2
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
import threading

sys.path.insert(0, '/home/arduino/.local/lib/python3.13/site-packages')
sys.path.insert(0, '/usr/local/lib/python3.13/dist-packages')

from ultralytics import YOLO

MODEL_PATH = 'best_ncnn_model'

model = YOLO(MODEL_PATH, task='segment')

def find_camera():
    for i in range(6):
        cap = cv2.VideoCapture(i, cv2.CAP_V4L2)

        if cap.isOpened():
            ret, frame = cap.read()

            if ret:
                print(f"Câmera encontrada em /dev/video{i}")
                return cap

        cap.release()

    raise RuntimeError("Nenhuma câmera encontrada")


cap = find_camera()

cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

frame = None
lock = threading.Lock()


def process_camera():
    global frame

    while True:
        ret, img = cap.read()

        if not ret:
            continue

        results = model(
            img,
            conf=0.8,
            imgsz=320,
            verbose=False
        )

        for r in results:
            img = r.plot()

        success, buffer = cv2.imencode('.jpg', img)

        if success:
            with lock:
                frame = buffer.tobytes()


class VideoHandler(BaseHTTPRequestHandler):

    def do_GET(self):

        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()

            with open('index.html', 'rb') as f:
                self.wfile.write(f.read())

        elif self.path == '/video':
            self.send_response(200)
            self.send_header(
                'Content-Type',
                'multipart/x-mixed-replace; boundary=frame'
            )
            self.end_headers()

            while True:

                with lock:
                    current_frame = frame

                if current_frame is None:
                    continue

                self.wfile.write(b'--frame\r\n')
                self.wfile.write(b'Content-Type: image/jpeg\r\n')
                self.wfile.write(
                    f'Content-Length: {len(current_frame)}\r\n\r\n'.encode()
                )
                self.wfile.write(current_frame)
                self.wfile.write(b'\r\n')


camera_thread = threading.Thread(
    target=process_camera,
    daemon=True
)

camera_thread.start()

server = HTTPServer(('0.0.0.0', 8000), VideoHandler)

print("Visualizador rodando na porta 8000")

server.serve_forever()