# include <wiringPi.h>
# include <stdio.h>
# include <stdint.h>

# define CLK 27  // CLK 對應 GPIO27
# define DIO 17  // DIO 對應 GPIO17

// 定義 TM1637 的指令碼
# define TM1637_CMD1 0x40
# define TM1637_CMD2 0xC0
# define TM1637_CMD3 0x88

// 7 段顯示字形表 (0-9 和空白) 帶小數點
uint8_t digitToSegment[] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x00  // 空白
};

// 顯示帶有小數點的數字，將小數點位置設置為 1 或 2（即顯示兩個點）
uint8_t digitToSegmentWithDot(uint8_t digit, int isDot) {
    if (isDot) {
        return digitToSegment[digit] | 0x80;  // 將小數點的控制位（0x80）加入數字
    } else {
        return digitToSegment[digit];
    }
}

// 設定 CLK 和 DIO 的狀態
void TM1637_start() {
    pinMode(DIO, OUTPUT);
    digitalWrite(DIO, LOW);
    delayMicroseconds(10);
    digitalWrite(CLK, LOW);
}

void TM1637_stop() {
    pinMode(DIO, OUTPUT);
    digitalWrite(CLK, LOW);
    delayMicroseconds(10);
    digitalWrite(DIO, LOW);
    delayMicroseconds(10);
    digitalWrite(CLK, HIGH);
    delayMicroseconds(10);
    digitalWrite(DIO, HIGH);
}

void TM1637_writeByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(CLK, LOW);
        if (data & 0x01) {
            digitalWrite(DIO, HIGH);
        } else {
            digitalWrite(DIO, LOW);
        }
        data >>= 1;
        delayMicroseconds(10);
        digitalWrite(CLK, HIGH);
        delayMicroseconds(10);
    }
    pinMode(DIO, INPUT);
    digitalWrite(CLK, LOW);
    delayMicroseconds(10);
    digitalWrite(CLK, HIGH);
    delayMicroseconds(10);
    pinMode(DIO, OUTPUT);
}

void TM1637_setBrightness(uint8_t brightness) {
    TM1637_start();
    TM1637_writeByte(TM1637_CMD3 | (brightness & 0x07));
    TM1637_stop();
}

void TM1637_displayDigits(uint8_t digits[4]) {
    TM1637_start();
    TM1637_writeByte(TM1637_CMD2);
    for (int i = 0; i < 4; i++) {
        TM1637_writeByte(digits[i]);
    }
    TM1637_stop();
}

void TM1637_displayTime(int seconds) {
    uint8_t digits[4];
    int minutes = seconds / 60;
    int secs = seconds % 60;
    
    // 將數字拆分為數位並加上小數點
    digits[0] = digitToSegmentWithDot(minutes / 10, 0);  // 顯示分鐘十位
    digits[1] = digitToSegmentWithDot(minutes % 10, 1);  // 顯示分鐘個位並加小數點
    digits[2] = digitToSegmentWithDot(secs / 10, 0);     // 顯示秒鐘十位
    digits[3] = digitToSegmentWithDot(secs % 10, 1);     // 顯示秒鐘個位並加小數點

    TM1637_displayDigits(digits);
}

int main() {
    // 初始化 wiringPi
    if (wiringPiSetupGpio() == -1) {
        printf("Setup wiringPi failed!");
        return -1;
    }

    pinMode(CLK, OUTPUT);
    pinMode(DIO, OUTPUT);

    // 設定亮度 (0x00 最暗, 0x07 最亮)
    TM1637_setBrightness(0x07);

    // 倒數計時從 2 分鐘 (120 秒) 開始
    int countdown = 120;

    while (countdown >= 0) {
        TM1637_displayTime(countdown);
        delay(1000); // 等待 1 秒
        countdown--;
    }

    // 倒數結束，顯示 0000
    uint8_t zeroDisplay[] = {0, 0, 0, 0};
    TM1637_displayDigits(zeroDisplay);

    return 0;
}
