import time
import tm1637

CLK = 22
DIO = 23
display = tm1637.TM1637(clk=CLK, dio=DIO)
display.brightness(7)
file_path = "game_score.txt"

counter = 1
try:
    while True:
        with open(file_path, "w") as file:
            file.write(str(counter))
        print(f"Written: {counter}") 
        display.show(str(counter).zfill(4))
        counter += 1
        time.sleep(1)  
except KeyboardInterrupt:
    print("\nterminated")
