#include <stdio.h>
#include <fcntl.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>   // signal()
#include <sys/wait.h> // waitpid()

#define DEVICE "/dev/car_device"

void zombie_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void disable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

int main(int argc, char *argv[]){
    
    signal(SIGCHLD, zombie_handler);
    pid_t pid = fork();
    if (pid == -1){
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }else if (pid == 0){
        printf("Starting stream_camera.py in child process...\n");
        execlp("python3", "python3", "stream_camera.py", (char *)NULL);

        perror("Failed to start stream_camera.py");
        exit(1);
    }else{
        // open device 
        int fd = open(DEVICE, O_WRONLY);
        if (fd == -1) {
            perror("Failed to open the device");
            return EXIT_FAILURE;
        }
    
        enable_raw_mode();
        system("clear"); // 
        printf("           W\n");
        printf("           |\n");
        printf("    A ---- | ---- D\n");
        printf("           |\n");
        printf("           S\n");
        printf("\nPress 'q' for quit...\n");

        char ch;
        ssize_t bytes_written;
        while(1){
            ch = getchar();
            if (ch == 'Q' || ch == 'q'){
                printf("Exiting ...\n");
                break;
            }
            if (ch == 'w' || ch == 'W' ||ch == 's' || ch == 'S'||ch == 'a' || ch == 'A'||ch == 'd' || ch == 'D'){
                bytes_written = write(fd, &ch , 1);
                usleep(300000);
                ch = 'x';
                write(fd, &ch , 1);
            }
        }

        disable_raw_mode();
        close(fd);
        return EXIT_SUCCESS;
    }
}