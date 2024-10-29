import cv2
import numpy as np

videoCapture = cv2.VideoCapture(0)

while True:
    ret, frame = videoCapture.read()
    if not ret:
        break

    # 轉灰階直方圖均衡化再高斯模糊
    grayFrame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    grayFrame = cv2.equalizeHist(grayFrame)
    blurFrame = cv2.GaussianBlur(grayFrame, (9, 9), 0)

    # 霍夫圓檢測 參數要調一下 circles回傳的是一個列表
    # dp分辨率 數字越大越不准
    circles = cv2.HoughCircles(blurFrame, cv2.HOUGH_GRADIENT, 0.8, 50,
                               param1=200, param2=35, minRadius=30, maxRadius=200)

    circle_count = 0
    if circles is not None:
        circles = np.round(circles[0, :]).astype("int")
        circle_count = len(circles)
        for circle in circles:
            x, y, radius = circle

            # 在影像上面標示圓
            cv2.circle(blurFrame, (x, y), 1, (0, 100, 100), 3)
            cv2.circle(blurFrame, (x, y), radius, (255, 0, 255), 3)

    cv2.imshow('Multiple Circles Detection', blurFrame)
    print("Circles detected", circle_count)
    if cv2.waitKey(1) == ord('q'):
        break

videoCapture.release()
cv2.destroyAllWindows()
