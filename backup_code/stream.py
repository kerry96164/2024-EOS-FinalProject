from flask import Flask, render_template_string, Response
from io import BufferedIOBase
from threading import Condition
from picamera2 import Picamera2
from picamera2.encoders import JpegEncoder, Quality
from picamera2.outputs import FileOutput
from libcamera import controls, Transform
import cv2
import numpy as np
from collections import deque
import tm1637
CLK = 22
DIO = 23
display = tm1637.TM1637(clk=CLK, dio=DIO)
display.brightness(7)
file_path = "game_score.txt"


template = '''
    <!DOCTYPE html>
    <html lang="zh-Hant-TW">
        <style>
            img {
                height: 80%;
                width: 60%;
                object-fit: contain;
            }
            body, html {
                margin: 0;
                padding: 0;
            }
        </style>
        <body>
            <img src="{{ url_for('video_stream') }}"">
        </body>
    </html>
    '''

# 初始化記錄圓數的緩衝區，長度為 10
circle_count_buffer = deque(maxlen=5)

app = Flask(__name__)


class StreamingOutput(BufferedIOBase):
    def __init__(self):
        self.frame = None
        self.condition = Condition()

    def write(self, buf):
        with self.condition:
            self.frame = buf
            self.condition.notify_all()


output = StreamingOutput()

def gen_frames():
    while True:
        with output.condition:
            output.condition.wait()
            frame = output.frame
        
        # 解碼圖片
        nparr = np.frombuffer(frame, np.uint8)  # 轉換為 NumPy 陣列
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

        # 裁切
        x, y, w, h = 210, 0, 1400, 1190  # ROI 的左上角 (x, y) 和寬高 (w, h)
        img = img[y:y+h, x:x+w]

        # 橘色篩選
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)  # 轉換為HSV顏色空間
        lower_orange = np.array([0, 10, 125])  # 定義橘色範圍
        upper_orange = np.array([60, 255, 255])
        mask = cv2.inRange(hsv, lower_orange, upper_orange)  # 創建遮罩
        masked_frame = cv2.bitwise_and(img, img, mask=mask)  # 應用遮罩，只保留橘色區域
        

        # 轉灰階直方圖均衡化再高斯模糊
        blurFrame = cv2.cvtColor(masked_frame, cv2.COLOR_BGR2GRAY)
        #grayFrame = cv2.equalizeHist(grayFrame)
        #blurFrame = cv2.GaussianBlur(grayFrame, (9, 9), 0)

        # 霍夫圓檢測 參數要調一下 circles回傳的是一個列表
        # dp分辨率 數字越大越不准
        circles = cv2.HoughCircles(blurFrame, cv2.HOUGH_GRADIENT, 1, 250,
                                param1=200, param2=10, minRadius=120, maxRadius=140)

        circle_count = 0
        if circles is not None:
            circles = np.round(circles[0, :]).astype("int")
            circle_count = len(circles)
            # 計算平均數
            circle_count_buffer.append(circle_count)
            avg_circle_count = np.round(sum(circle_count_buffer) / len(circle_count_buffer)).astype("int")  
            # 圖片加上數字
            cv2.putText(blurFrame, str(avg_circle_count),
                         org=(20,90), fontFace = cv2.FONT_HERSHEY_SIMPLEX,
                         fontScale = 2.5,
                         color = (255,255,255), 
                         thickness = 5, 
                         lineType = cv2.LINE_AA
                         )
            for circle in circles:
                x, y, radius = circle
                # 在影像上面標示圓
                cv2.circle(blurFrame, (x, y), 1, (0, 100, 100), 3)
                cv2.circle(blurFrame, (x, y), radius, (255, 0, 255), 3)

        blurFrame_colored = cv2.cvtColor(blurFrame, cv2.COLOR_GRAY2BGR)
        # 編碼回 JPEG
        result_frame = np.vstack((img, masked_frame, blurFrame_colored))
        _, buffer = cv2.imencode('.jpg', result_frame)
        result = buffer.tobytes()

        yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + result + b'\r\n')


def detect_ball():
   # while True:
        with output.condition:
            output.condition.wait()
            frame = output.frame
        
        # 解碼圖片
        nparr = np.frombuffer(frame, np.uint8)  # 轉換為 NumPy 陣列
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

        # 裁切
        x, y, w, h = 210, 0, 1400, 1190  # ROI 的左上角 (x, y) 和寬高 (w, h)
        img = img[y:y+h, x:x+w]

        # 橘色篩選
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)  # 轉換為HSV顏色空間
        lower_orange = np.array([0, 10, 125])  # 定義橘色範圍
        upper_orange = np.array([60, 255, 255])
        mask = cv2.inRange(hsv, lower_orange, upper_orange)  # 創建遮罩
        masked_frame = cv2.bitwise_and(img, img, mask=mask)  # 應用遮罩，只保留橘色區域
        

        # 轉灰階直方圖均衡化再高斯模糊
        blurFrame = cv2.cvtColor(masked_frame, cv2.COLOR_BGR2GRAY)
        #grayFrame = cv2.equalizeHist(grayFrame)
        #blurFrame = cv2.GaussianBlur(grayFrame, (9, 9), 0)

        # 霍夫圓檢測 參數要調一下 circles回傳的是一個列表
        # dp分辨率 數字越大越不准
        circles = cv2.HoughCircles(blurFrame, cv2.HOUGH_GRADIENT, 1, 250,
                                param1=200, param2=10, minRadius=120, maxRadius=140)

        circle_count = 0
        if circles is not None:
            circles = np.round(circles[0, :]).astype("int")
            circle_count = len(circles)
        else:
            circle_count = 0
        # 計算平均數
        circle_count_buffer.append(circle_count)
        avg_circle_count = np.round(sum(circle_count_buffer) / len(circle_count_buffer)).astype("int")
        with open(file_path, "w") as file:
            file.write(str(avg_circle_count))
        print(avg_circle_count)
        display.show(str(avg_circle_count).zfill(4))
    

@app.route("/", methods=['GET'])
def get_stream_html():
    return render_template_string(template)


@app.route('/api/stream')
def video_stream():
    return Response(gen_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')


if __name__ == "__main__":
    cam = Picamera2()
    config = cam.create_video_configuration(
        {'size': (1672, 1254), 'format': 'RGB888'},
        #transform=Transform(vflip=1),
        controls={'NoiseReductionMode': controls.draft.NoiseReductionModeEnum.Off, 'Sharpness': 1.5}
    )
    cam.configure(config)
    cam.start_recording(JpegEncoder(), FileOutput(output), Quality.MEDIUM)
    # only detect_ball
    detect_ball()
    # web
    #app.run(host='0.0.0.0')

    cam.stop()