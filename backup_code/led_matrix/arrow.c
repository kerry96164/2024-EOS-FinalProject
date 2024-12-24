#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

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

void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &new_termios);
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

void reset_terminal_mode() {
    struct termios old_termios;
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
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

int main() {
    
    int fd = open(SPI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open SPI device");
        return 1;
    }

    uint8_t mode = 0;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("Failed to set SPI parameters");
        return 1;
    }

    max7219_write(fd, MAX7219_REG_SHUTDOWN, 0x01);
    max7219_write(fd, MAX7219_REG_DECODEMODE, 0x00);
    max7219_write(fd, MAX7219_REG_SCANLIMIT, 0x07);
    max7219_write(fd, MAX7219_REG_INTENSITY, 0x08);
    max7219_write(fd, MAX7219_REG_DISPLAYTEST, 0x00);

    set_conio_terminal_mode();

    printf("Press W/A/S/D to display arrows. Press Q to quit.\n");

    char ch;
    while ((ch = getchar()) != 'q') {
        switch (ch) {
            case 'w':
                display_arrow(fd, arrow_up);
                break;
            case 'a':
                display_arrow(fd, arrow_left);
                break;
            case 's':
                display_arrow(fd, arrow_down);
                break;
            case 'd':
                display_arrow(fd, arrow_right);
                break;
        }
    }

    reset_terminal_mode();
    close(fd);
    return 0;
}