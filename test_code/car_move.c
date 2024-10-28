#include <stdio.h>
#include <fcntl.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <termios.h>

#define DEVICE "/dev/car_device"

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


int main(int argc,char *argv[]) {

    
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
            sleep(1);
            ch = 'x';
            write(fd, &ch , 1);
        }
    }

    disable_raw_mode();
    close(fd);
    return EXIT_SUCCESS;
}
