from tkinter import *
import customtkinter
from PIL import Image
import os
import time
from multiprocessing import Process
import subprocess

DEVICE = "/dev/car_device"
fd = os.open(DEVICE, os.O_WRONLY)

def run_camera_stream():
    subprocess.run(['python3', 'camera_stream.py'])

def controll_car(ch):
    os.write(fd, ch.encode())
    time.sleep(0.3)  # Delay
    os.write(fd, b'x')

p = Process(target=run_camera_stream)
p.start()


# Set theme
customtkinter.set_appearance_mode("light")

# creating tkinter window
root = customtkinter.CTk()
root.title('Remote Car Controller')
root.geometry('300x350')
root.resizable(False, False)

# Set frame
frame = customtkinter.CTkFrame(master=root, width=230, height=230, corner_radius=10)
frame.place(relx=0.5, rely=0.6, anchor= CENTER)

# Set Label
label = customtkinter.CTkLabel(master = root, text = "Game START!!!",text_color="#A52A2A", fg_color = "#ADD8E6", width=100, height=50, corner_radius=10)
label.place(relx=0.5, rely=0.1, anchor= CENTER)

# Set button
left_arrow_image = customtkinter.CTkImage(Image.open("left_arrow.png"), size=(30, 30))
button_left = customtkinter.CTkButton(master=root, command=lambda: controll_car('a'), image=left_arrow_image, fg_color="#ADD8E6", text=None, width=30, height=30)
button_left.place(relx=0.24, rely=0.6, anchor=CENTER)

right_arrow_image = customtkinter.CTkImage(Image.open("right_arrow.png"), size=(30, 30))
button_right = customtkinter.CTkButton(master=root, command=lambda: controll_car('d'), image=right_arrow_image, fg_color="#ADD8E6", text=None, width=30, height=30)
button_right.place(relx=0.76, rely=0.6, anchor=CENTER)

up_arrow_image = customtkinter.CTkImage(Image.open("up_arrow.png"), size=(30, 30))
button_up = customtkinter.CTkButton(master=root, command=lambda: controll_car('w'), image=up_arrow_image, fg_color="#ADD8E6", text=None, width=30, height=30)
button_up.place(relx=0.5, rely=0.35, anchor=CENTER)

down_arrow_image = customtkinter.CTkImage(Image.open("down_arrow.png"), size=(30, 30))
button_down = customtkinter.CTkButton(master=root, command=lambda: controll_car('s'), image=down_arrow_image, fg_color="#ADD8E6", text=None, width=30, height=30)
button_down.place(relx=0.5, rely=0.85, anchor=CENTER)

# Execute Tkinter
root.mainloop()
p.join()