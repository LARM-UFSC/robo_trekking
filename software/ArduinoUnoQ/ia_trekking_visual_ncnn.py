import cv2
import ncnn
import numpy as np
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import threading
import time

MODEL = "/home/arduino/robo_trekking/software/VisãoComputacional/best_ncnn_model"

net = ncnn.Net()
net.load_param(MODEL + "/model.ncnn.param")
net.load_model(MODEL + "/model.ncnn.bin")

cap = cv2.VideoCapture("/dev/video2")
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

latest_jpeg = None
lock = threading.Lock()


def detectar(frame):
    img = cv2.resize(frame, (320, 320))

    mat = ncnn.Mat.from_pixels(
        img,
        ncnn.Mat.PixelType.PIXEL_BGR,
        320,
        320
    )

    with net.create_extractor() as ex:
        ex.input("in0", mat)
        ret, out = ex.extract("out0")

    if ret != 0:
        return frame

    data = np.array(out)

    if data.ndim == 3:
        data = data.reshape(data.shape[0], data.shape[1])

    # saída: 5 x 2100
    boxes = []
    scores = []

    for i in range(data.shape[1]):
        cx, cy, w, h, conf = data[:, i]

        if conf < 0.5:
            continue

        x = (cx - w / 2) * 640 / 320
        y = (cy - h / 2) * 480 / 320
        w = w * 640 / 320
        h = h * 480 / 320

        boxes.append([int(x), int(y), int(w), int(h)])
        scores.append(float(conf))

    if boxes:
        indices = cv2.dnn.NMSBoxes(
            boxes,
            scores,
            0.5,
            0.45
        )

        if len(indices) > 0:
            for idx in np.array(indices).flatten():
                x, y, w, h = boxes[idx]

                cv2.rectangle(
                    frame,
                    (x, y),
                    (x + w, y + h),
                    (0, 255, 0),
                    2
                )

                cv2.putText(
                    frame,
                    f"cone {scores[idx]:.2f}",
                    (x, max(20, y - 5)),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    (0, 255, 0),
                    1
                )

    return frame


def camera_loop():
    global latest_jpeg

    while True:
        ret, frame = cap.read()

        if not ret:
            time.sleep(0.1)
            continue

        frame = detectar(frame)

        ok, jpeg = cv2.imencode(
            ".jpg",
            frame,
            [cv2.IMWRITE_JPEG_QUALITY, 80]
        )

        if ok:
            with lock:
                latest_jpeg = jpeg.tobytes()


class Handler(BaseHTTPRequestHandler):

    def do_GET(self):

        if self.path == "/":
            html = b"""
            <html>
            <head>
                <title>Trekking Vision</title>
            </head>
            <body style="background:#111;text-align:center;">
                <h2 style="color:white;">Trekking Vision</h2>
                <img src="/video" style="max-width:100%;">
            </body>
            </html>
            """

            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)

        elif self.path == "/video":

            self.send_response(200)
            self.send_header(
                "Content-Type",
                "multipart/x-mixed-replace; boundary=frame"
            )
            self.end_headers()

            while True:
                with lock:
                    jpeg = latest_jpeg

                if jpeg:
                    try:
                        self.wfile.write(b"--frame\r\n")
                        self.wfile.write(b"Content-Type: image/jpeg\r\n")
                        self.wfile.write(
                            f"Content-Length: {len(jpeg)}\r\n\r\n".encode()
                        )
                        self.wfile.write(jpeg)
                        self.wfile.write(b"\r\n")
                    except:
                        break

                time.sleep(0.03)


threading.Thread(
    target=camera_loop,
    daemon=True
).start()

print("Servidor em http://0.0.0.0:8000")

server = ThreadingHTTPServer(
    ("0.0.0.0", 8000),
    Handler
)

server.serve_forever()
