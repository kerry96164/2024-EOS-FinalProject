#include <stdio.h>
#include <fcntl.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>   // signal()
#include <sys/wait.h> // waitpid()
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>

#define DEVICE "/dev/car_device"
#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_SPEED 1000000

#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DECODEMODE  0x09
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_DISPLAYTEST 0x0F

uint8_t arrow_left[8] = {
    0x18, 0x3C, 0x7E, 0xDB, 0x18, 0x18, 0x18, 0x18
};

uint8_t arrow_right[8] = {
    0x18, 0x18, 0x18, 0x18, 0xDB, 0x7E, 0x3C, 0x18
};

uint8_t arrow_down[8] = {
    0x10, 0x30, 0x60, 0xFF, 0xFF, 0x60, 0x30, 0x10
};

uint8_t arrow_up[8] = {
    0x08, 0x0C, 0x06, 0xFF, 0xFF, 0x06, 0x0C, 0x08
};


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

void max7219_write(int fd, uint8_t reg, uint8_t data) {
    uint8_t tx[] = {reg, data};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = 0,
        .len = 2,
        .speed_hz = SPI_SPEED,
        .bits_per_word = 8,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("SPI transfer failed");
        exit(1);
    }
}

void display_arrow(int fd, uint8_t *arrow) {
    for (int row = 0; row < 8; row++) {
        max7219_write(fd, MAX7219_REG_DIGIT0 + row, arrow[row]);
    }
}

int main(int argc, char *argv[]){
    enable_raw_mode();
    signal(SIGCHLD, zombie_handler);
    pid_t pid = fork();
    if (pid == -1){
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }else if (pid == 0){
        printf("Starting stream_camera.py in child process...\n");
        execlp("python3", "python3", "camera_stream.py", (char *)NULL);

        perror("Failed to start camera_stream.py");
        exit(1);
    }else{
        // open device 
        int fd = open(DEVICE, O_WRONLY);
        if (fd == -1) {
            perror("Failed to open the device");
            return EXIT_FAILURE;
        }
    
        int fd2 = open(SPI_DEVICE, O_RDWR);
        if (fd2 < 0) {
            perror("Failed to open SPI device");
            return 1;
        }

        uint8_t mode = 0;
        uint8_t bits = 8;
        uint32_t speed = SPI_SPEED;
        if (ioctl(fd2, SPI_IOC_WR_MODE, &mode) == -1 ||
            ioctl(fd2, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1 ||
            ioctl(fd2, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
            perror("Failed to set SPI parameters");
            return 1;
        }

        max7219_write(fd2, MAX7219_REG_SHUTDOWN, 0x01);
        max7219_write(fd2, MAX7219_REG_DECODEMODE, 0x00);
        max7219_write(fd2, MAX7219_REG_SCANLIMIT, 0x07);
        max7219_write(fd2, MAX7219_REG_INTENSITY, 0x08);
        max7219_write(fd2, MAX7219_REG_DISPLAYTEST, 0x00);
        
        system("clear"); // 
        printf("           W\n");
        printf("           |\n");
        printf("    A ---- | ---- D\n");
        printf("           |\n");
        printf("           S\n");
        printf("\nPress 'q' for quit...\n");

        char ch;
        while(1){
            ch = getchar();
            if (ch == 'Q' || ch == 'q'){
                printf("Exiting ...\n");
                break;
            }
            if (ch == 'w' || ch == 'W'){
                write(fd, &ch , 1);
                display_arrow(fd2, arrow_up);
                usleep(300000);
                ch = 'x';
                write(fd, &ch , 1);
            }else if(ch == 'a'|| ch == 'A'){
                write(fd, &ch , 1);
                display_arrow(fd2, arrow_left);
                usleep(300000);
                ch = 'x';
                write(fd, &ch , 1);
            }else if(ch == 's'|| ch == 'S'){
                write(fd, &ch , 1);
                display_arrow(fd2, arrow_down);
                usleep(300000);
                ch = 'x';
                write(fd, &ch , 1);
            }else if(ch == 'd'|| ch == 'D'){
                write(fd, &ch , 1);
                display_arrow(fd2, arrow_right);
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