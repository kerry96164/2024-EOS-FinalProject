#! /bin/bash

# Will auto run this bash when boot
cd /home/pi/car_web_server

sudo rmmod car_driver
sudo insmod car_driver.ko

echo "$(date) - Starting car_server..." >> /home/pi/car_web_server/log.txt
./car_server >> /home/pi/car_web_server/log.txt 2>&1
echo "$(date) - goal_server finished." >> /home/pi/car_web_server/log.txt