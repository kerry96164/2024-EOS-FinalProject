import time

file_path = "game_score.txt"

counter = 1
try:
    while True:
        with open(file_path, "w") as file:
            file.write(str(counter))
        print(f"Written: {counter}") 
        counter += 1
        time.sleep(1)  
except KeyboardInterrupt:
    print("\nterminated")
