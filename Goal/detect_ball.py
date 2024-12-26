#from io import BufferedIOBase
#from threading import Condition
from picamera2 import Picamera2
#from picamera2.encoders import JpegEncoder, Quality
#from picamera2.outputs import FileOutput
#from libcamera import controls, Transform
import cv2
import numpy as np
#from collections import deque
import tm1637
import time
import signal
CLK = 22
DIO = 23
display = tm1637.TM1637(clk=CLK, dio=DIO)
display.brightness(7)
file_path = "game_score.txt"
RUN = True
dot = True

# 初始化記錄圓數的緩衝區，長度為 10
# circle_count_buffer = deque(maxlen=5)

def detect_ball(cam):
    frame = cam.capture_array()
        
    # 解碼圖片
    # img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

    # 裁切
    x, y, w, h = 210, 0, 1400, 1190  # ROI 的左上角 (x, y) 和寬高 (w, h)
    img = frame[y:y+h, x:x+w]

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
                            param1=100, param2=15, minRadius=120, maxRadius=140)

    circle_count = 0
    if circles is not None:
        circles = np.round(circles[0, :]).astype("int")
        circle_count = len(circles)
    else:
        circle_count = 0
    # 計算平均數(棄用，一秒拍一張會變太慢)
    # circle_count_buffer.append(circle_count)
    # avg_circle_count = np.round(sum(circle_count_buffer) / len(circle_count_buffer)).astype("int")
    avg_circle_count = circle_count
    with open(file_path, "w") as file:
        file.write(str(avg_circle_count))
    #print(avg_circle_count)
    global dot
    display.show(f"   {avg_circle_count}"[-4:], colon=dot) # 顯示在四位數七段顯示器上，不足四位補空格，並顯示冒號，冒號會閃爍
    dot = not dot

def handler(signum, frame):
    #signame = signal.Signals(signum).name
    #print(f'\nSignal handler called with signal {signame} ({signum})\n')
    global RUN
    RUN = False

signal.signal(signal.SIGINT, handler)
    
    
if __name__ == "__main__":
    cam = Picamera2()
    config = cam.create_still_configuration(
        {'size': (1672, 1254), 'format': 'RGB888'}
        #,transform=Transform(vflip=1)
        #,controls={'NoiseReductionMode': controls.draft.NoiseReductionModeEnum.Off, 'Sharpness': 1.5}
    )
    cam.configure(config)
    cam.start()
    try:
        while(RUN):
            detect_ball(cam)
            time.sleep(1)
    except Exception as e:
        print(f"Error: {e}")
    finally:
        cam.stop()
        display.show('    ')

