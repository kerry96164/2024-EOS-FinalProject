#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_SPEED 1000000

// MAX7219 註冊位址
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_DIGIT1      0x02
#define MAX7219_REG_DIGIT2      0x03
#define MAX7219_REG_DIGIT3      0x04
#define MAX7219_REG_DIGIT4      0x05
#define MAX7219_REG_DIGIT5      0x06
#define MAX7219_REG_DIGIT6      0x07
#define MAX7219_REG_DIGIT7      0x08
#define MAX7219_REG_DECODEMODE  0x09
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DISPLAYTEST 0x0F

// 數字矩陣（0~9），每個數字占 8x8 的矩陣（每位為一列）
uint8_t digit_matrix[10][8] = {
    {0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C}, // 0
    {0x00, 0x00, 0x82, 0xFF, 0xFF, 0x80, 0x00, 0x00}, // 1
    {0xC2, 0xA1, 0x91, 0x91, 0x89, 0x89, 0x86, 0x00}, // 2
    {0x42, 0x81, 0x89, 0x89, 0x89, 0x89, 0x76, 0x00}, // 3
    {0x10, 0x18, 0x14, 0x12, 0xFF, 0xFF, 0x10, 0x10}, // 4
    {0x4F, 0x89, 0x89, 0x89, 0x89, 0x89, 0x71, 0x00}, // 5
    {0x7E, 0x91, 0x89, 0x89, 0x89, 0x89, 0x71, 0x00}, // 6
    {0x01, 0x01, 0xF1, 0xF9, 0x09, 0x05, 0x03, 0x00}, // 7
    {0x76, 0x89, 0x89, 0x89, 0x89, 0x89, 0x76, 0x00}, // 8
    {0x46, 0x89, 0x89, 0x89, 0x89, 0x89, 0x7E, 0x00}  // 9
};

// 發送資料到 MAX7219
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

// 顯示矩陣上的數字
void display_digit(int fd, uint8_t digit) {
    for (int row = 0; row < 8; row++) {
        max7219_write(fd, MAX7219_REG_DIGIT0 + row, digit_matrix[digit][row]);
    }
}

int main() {
    int fd = open(SPI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open SPI device");
        return 1;
    }

    // 初始化 SPI
    uint8_t mode = 0;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("Failed to set SPI parameters");
        return 1;
    }

    // MAX7219 初始化
    max7219_write(fd, MAX7219_REG_SHUTDOWN, 0x01);    // 正常操作模式
    max7219_write(fd, MAX7219_REG_DECODEMODE, 0x00);  // 關閉 BCD 解碼
    max7219_write(fd, MAX7219_REG_SCANLIMIT, 0x07);   // 顯示 8 列
    max7219_write(fd, MAX7219_REG_INTENSITY, 0x08);   // 中等亮度
    max7219_write(fd, MAX7219_REG_DISPLAYTEST, 0x00); // 測試模式關閉

    // 從 1 顯示到 9
    for (uint8_t i = 1; i <= 9; i++) {
        display_digit(fd, i); // 顯示數字
        sleep(1);             // 延遲 1 秒
    }

    close(fd);
    return 0;
}
