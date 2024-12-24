# 記得開虛擬環境
import time
import tm1637

display = tm1637.TM1637(clk=27, dio=17)

def display_number(num):
    str_num = str(num).zfill(4) 

    display.show(str_num)


if __name__ == "__main__":
    while True:
        for i in range(10000):
            display_number(i)
            time.sleep(1)  
