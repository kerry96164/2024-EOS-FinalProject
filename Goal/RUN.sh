#! /bin/bash

# Will auto run this bash when boot
echo "$(date) - Starting goal_server..." >> /home/pi/Goal/log.txt
/home/pi/Goal/goal_server 8888 >> /home/pi/Goal/log.txt 2>&1
echo "$(date) - goal_server finished." >> /home/pi/Goal/log.txt