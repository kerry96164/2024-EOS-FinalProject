# 2024-EOS-FinalProject
1131 Embedded Operating Systems 嵌入式作業系統 Final Project  
NYCU EECN30062

## Environment & Equipments
* PC (Ubuntu 22.04)
* Car 
    * Raspberry Pi 4 Model B
    * Raspberry Pi Camera Moudule V2
    * DC Motor (FA-130RA)
    * L298N Motor Driver
    * Bettery
    * LED Matrix (MAX7219)
* Goal
    * Raspberry Pi 4 Model B
    * Raspberry Pi Camera Moudule V2
    * 4-digit Display (TM1637) *2

## Structure
### Car `car_web_server/car_server.c`
[Demo](https://youtu.be/TJ8jm6rcYDU)  
* Multiple players can control simultaneously.
* Use Semaphore to prevent race conditions.
* The motor driver uses PWM to achieve different speeds.
* The Flask web server takes player input and sends back the camera view.
* Use sockets to obtain scores from the goal.
* Display movement direction using an LED matrix.
### Goal: `Goal/goal_server.c`  
[Demo](https://youtu.be/c0X0LgzIEgA)  
* Execute `detect_ball.py` to count the number of balls in the goal.
* Controls game flow and tracks scores.
* Forks child processes to handle car connections.
#### Ball Detect: `Goal/detect_ball.py`
[Demo](https://youtu.be/OWKXxmOcbDw)
* Using color filter and Hough Circle Transform  
## Full Demo
[Demo Video](https://youtu.be/pdQKvCO1ucs)

> For more details, please see [Report](/ppt/EOS_Final.pdf)
