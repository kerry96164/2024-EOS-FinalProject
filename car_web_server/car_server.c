/*
 * car_server.c
 * receiver socket client request then control motor driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <sys/ioctl.h>

#define PORT 4534
#define NUM_CLIENT 5

// LED Matrix
#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_SPEED 1000000
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DECODEMODE  0x09
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_INTENSITY   0x01
#define MAX7219_REG_DISPLAYTEST 0x0F
uint8_t arrow_left[8] = { 0x18, 0x3C, 0x7E, 0xDB, 0x18, 0x18, 0x18, 0x18 };
uint8_t arrow_right[8] = { 0x18, 0x18, 0x18, 0x18, 0xDB, 0x7E, 0x3C, 0x18 };
uint8_t arrow_down[8] = { 0x10, 0x30, 0x60, 0xFF, 0xFF, 0x60, 0x30, 0x10 };
uint8_t arrow_up[8] = { 0x08, 0x0C, 0x06, 0xFF, 0xFF, 0x06, 0x0C, 0x08 };
uint8_t arrow_stop[8] = { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 };
uint8_t arrow_lower_left[8] = { 0xFC, 0xF8, 0xF0, 0xF8, 0xDC, 0x8E, 0x07, 0x03 };
uint8_t arrow_upper_left[8] = { 0x3F,0x1F,0x0F,0x1F,0x3B,0x71,0xE0,0xC0 };
uint8_t arrow_lower_right[8] = { 0x03,0x07,0x8E,0xDC,0xF8,0xF0,0xF8,0xFC };
uint8_t arrow_upper_right[8] = { 0xC0,0xE0,0x71,0x3B,0x1F,0x0F,0x1F,0x3F };
uint8_t none[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// Motor Driver
#define MAX_SPEED 100   // PWM 100%
#define MIN_SPEED 40    // PWM 40%
#define MOTOR_DEVICE "/dev/car_device"
// typedef struct {
//     int right;
//     int left;
// } wheel_speed_t;

typedef struct {
    int right;
    int left;
} shared_data_t;

int shm_size = sizeof(shared_data_t);
shared_data_t *shm_ptr; // shared memory pointer (used for speed)

int server_sockfd = -1;
int reply_sockfd = -1;
struct sockaddr_in server_addr, client_addr;
int client_addr_len = sizeof(client_addr);
int shm_id = -1;
int sem_id = -1;
pid_t webcam_pid = -1;
pid_t motor_controler_pid = -1;
pid_t child_pid = -1;

void close_handler();
void sigint_handler(int signum);
void zombie_handler(int signum);
void create_socket();
void create_shared_memory();
void create_semaphore();
void update_speed_from_socket();
void motor_controler();
void enable_raw_mode();
void disable_raw_mode();
void max7219_write(int fd, uint8_t reg, uint8_t data);
void display_arrow(int fd, uint8_t *arrow);
int open_LED_matrix();
void display_LED(int fd, uint8_t *arrow);
void send_direction_command(char buffer[]);
void send_pwm_command(int left, int right);

int P(int s, int n);
int V(int s, int n);

int main(int argc, char *argv[]) {
    signal(SIGCHLD, zombie_handler); // SIGCHLD: child process terminated
    signal(SIGINT, sigint_handler); // SIGINT： Ctrl-C
    atexit(close_handler); // register atexit function

    webcam_pid = fork();
    if (webcam_pid < 0) {
        perror("fork");
        exit(-1);
    } else if(webcam_pid == 0) {
        execl("/usr/bin/python3", "python3", "web_server.py", NULL);
        perror("execl");
        exit(-1);
    }

    create_shared_memory();
    create_semaphore();  

    motor_controler_pid = fork();
    if (motor_controler_pid < 0) {
        perror("fork");
        exit(-1);
    } else if(motor_controler_pid == 0) {
        motor_controler();
        exit(-1);
    }

    create_socket();

    while (1){
        reply_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (reply_sockfd < 0) {
            perror("accept");
            exit(-1);
        }
        printf("client [%s:%d] --- connect\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        if((child_pid = fork()) == 0){
            // child process
            //close(server_sockfd);
            update_speed_from_socket();
            exit(0);
        }else if (child_pid < 0){
            perror("fork");
            exit(-1);
        }else{
            close(reply_sockfd);
            reply_sockfd = -1;
        }
        
    }
    return 0;
}

void motor_controler(){
    // 讀取所有shared memory的值，並控制馬達

    int fd_motor;
    if((fd_motor = open(MOTOR_DEVICE, O_RDWR)) < 0){
        perror("open fd_motor");
        exit(-1);
    }
    int fd_LED_matrix = open_LED_matrix();
    if (fd_LED_matrix < 0) {
        perror("open_LED_matrix");
        exit(-1);
    }
    display_LED(fd_LED_matrix, none);

    int left_speed = 0;
    int right_speed = 0;
    int speed_range = MAX_SPEED - MIN_SPEED;
    int flag = 0;
    while(1){
        P(sem_id, 0);
        P(sem_id, 1);
        left_speed = shm_ptr->left;
        right_speed = shm_ptr->right;
        V(sem_id, 0);
        V(sem_id, 1);

        if(left_speed > 0){
            send_direction_command("lf");
        }else if (left_speed < 0){
            send_direction_command("lb");
        }
        if(right_speed > 0){
            send_direction_command("rf");
        }else if (right_speed < 0){
            send_direction_command("rb");
        }
        if(left_speed == 0 && right_speed == 0){
            send_direction_command("stop");
        }else{
            send_pwm_command(MIN_SPEED + abs(left_speed) * speed_range / 50,
                             MIN_SPEED + abs(right_speed) * speed_range / 50);
        }
        if (left_speed > 0 && right_speed > 0){
            if (left_speed > right_speed){
                display_LED(fd_LED_matrix, arrow_upper_right);
            }else if (left_speed < right_speed){
                display_LED(fd_LED_matrix, arrow_upper_left);
            }else{
                display_LED(fd_LED_matrix, arrow_up);
            }
        }else if (left_speed < 0 && right_speed < 0){
            if (left_speed > right_speed){
                display_LED(fd_LED_matrix, arrow_lower_right);
            }else if (left_speed < right_speed){
                display_LED(fd_LED_matrix, arrow_lower_left);
            }else{
                display_LED(fd_LED_matrix, arrow_down);
            }
        }else if (left_speed > 0 && right_speed <= 0){
            display_LED(fd_LED_matrix, arrow_right);
        }else if (left_speed <= 0 && right_speed > 0){
            display_LED(fd_LED_matrix, arrow_left);
        }else if (left_speed < 0 && right_speed == 0){
            display_LED(fd_LED_matrix, arrow_upper_right);
        }else if (left_speed == 0 && right_speed < 0){
            display_LED(fd_LED_matrix, arrow_upper_left);
        }else{ // left_speed == 0 && right_speed == 0
            if (flag >= 10){
                display_LED(fd_LED_matrix, arrow_stop);
                flag = 0;
            }else{
                display_LED(fd_LED_matrix, none);
                flag+=1;
            }
        }      

        usleep(100000); // 100ms
    }
    close(fd_motor);
}

// control motor driver
void send_direction_command(char buffer[]) {
    int fd = open(MOTOR_DEVICE, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open MOTOR_DEVICE");
        return;
    }
    write(fd, buffer, strlen(buffer));
    close(fd);
}
void send_pwm_command(int left, int right) {
    int fd = open(MOTOR_DEVICE, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open MOTOR_DEVICE");
        return;
    }
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "pwm %d %d", left, right);
    write(fd, buffer, strlen(buffer));
    close(fd);
}

void update_speed_from_socket(){
    // 接收client的request，並更新shared memory的值
    int left = 0;
    int right = 0;
    char buffer[1024];
    int content_length = 0;
    char body[1024];
    char ch;
    //while(1){
        int read_ret = read(reply_sockfd, buffer, sizeof(buffer));
        if( read_ret < 0){
            perror("read");
            exit(-1);
        }
        if(read_ret == 0){ // client disconnect
            printf("client [%s:%d] --- disconnect\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            exit(0);
        }
        buffer[read_ret] = '\0';
        // Find Content-Length
        char *content_length_header = strstr(buffer, "Content-Length: ");
        if (content_length_header) {
            sscanf(content_length_header, "Content-Length: %d", &content_length);
        }
        // Find body
        if (content_length > 0) {
            const char *start = &buffer[0] + read_ret - content_length;
            strncpy(body, start, content_length+1);
            body[content_length] = '\0';
        } 
        printf("Received: %s\n", body);
        
        sscanf(body, "%s", &ch);
        switch (ch){
            case 'w':
            case 'W':
                left = 1;
                right = 1;
                break;
            case 's':
            case 'S':
                left = -1;
                right = -1;
                break;
            case 'a':
            case 'A':
                left = -1;
                right = 1;
                break;
            case 'd':
            case 'D':
                left = 1;
                right = -1;
                break;
            case 'x':
            case 'X':
                left = 0;
                right = 0;
                P(sem_id, 0);
                P(sem_id, 1);
                shm_ptr->left = 0;
                shm_ptr->right = 0;
                V(sem_id, 0);
                V(sem_id, 1);
            default:
                break;
        }
        // write to shared memory
        if(left != 0){
            P(sem_id, 0);
            if (shm_ptr->left + left > 50){
                shm_ptr->left = 50;
            }else if(shm_ptr->left + left < -50){
                shm_ptr->left = -50;
            }else{
                shm_ptr->left += left;
            }
            V(sem_id, 0);
        }
        if(right != 0){
            P(sem_id, 1);
            if (shm_ptr->right + right > 50){
                shm_ptr->right = 50;
            }else if(shm_ptr->right + right < -50){
                shm_ptr->right = -50;
            }else{
                shm_ptr->right += right;
            }
            V(sem_id, 1);
        }
        char response[] = "HTTP/1.1 200 OK\r\n"
                        "Access-Control-Allow-Origin: http://192.168.1.2:5000\r\n"
                        "Content-Type: text/plain\r\n\r\n"
                        "Message received successfully";
        ssize_t sent_bytes = write(reply_sockfd, response, strlen(response));
        if (sent_bytes < 0) {
            perror("send");
            exit(-1);
        }
        exit(0);
    //}
}

// open LED matrix
int open_LED_matrix(){
    int fd_LED_matrix;
    if((fd_LED_matrix = open(SPI_DEVICE, O_RDWR)) < 0){
        perror("open fd_LED_matrix");
        exit(-1);
    }
    uint8_t mode = 0;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED;
    if (ioctl(fd_LED_matrix, SPI_IOC_WR_MODE, &mode) == -1 ||
        ioctl(fd_LED_matrix, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1 ||
        ioctl(fd_LED_matrix, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("Failed to set SPI parameters");
        return 1;
    }
    max7219_write(fd_LED_matrix, MAX7219_REG_SHUTDOWN, 0x01);
    max7219_write(fd_LED_matrix, MAX7219_REG_DECODEMODE, 0x00);
    max7219_write(fd_LED_matrix, MAX7219_REG_SCANLIMIT, 0x07);
    max7219_write(fd_LED_matrix, MAX7219_REG_INTENSITY, 0x08);
    max7219_write(fd_LED_matrix, MAX7219_REG_DISPLAYTEST, 0x00);
    return fd_LED_matrix;
}

// write to LED matrix
void display_LED(int fd, uint8_t *arrow) {
    for (int row = 0; row < 8; row++) {
        max7219_write(fd, MAX7219_REG_DIGIT0 + row, arrow[row]);
    }
}

// Handles the SIGINT signal (Ctrl-C) for graceful shutdown
void sigint_handler(int signum) {
    printf("\n Received interrupt signal (Ctrl-C). \nExiting program safely...\n");
    send_direction_command("stop");
    send_pwm_command(0, 0);
    display_LED(open_LED_matrix(), none);
    exit(0);
}
// SIGCHLD: child process terminated
void zombie_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
// called by atexit()
void close_handler(){
    // close socket
    if(server_sockfd>=0){
        if(close(server_sockfd)==0){
            printf("Socket %d closed.\n", server_sockfd);
        }else{
            perror("Failed to close socket");
        }
        server_sockfd = -1;
    }
    if(reply_sockfd>=0){
        if(close(reply_sockfd)==0){
            printf("Socket %d closed.\n", reply_sockfd);
        }else{
            perror("Failed to close socket");
        }
        reply_sockfd = -1;
    }
    printf("Socket closed.\n");

    if(child_pid&webcam_pid&motor_controler_pid){ // parent process
        // remove shared memory
        if (shm_id >= 0){
            if(shmctl(shm_id, IPC_RMID, NULL) >=0 ){
                printf("Removed shared memory %d\n", shm_id);
            }else{
                perror("Failed to remove shared memory");
            }
            shm_id = -1;
        }
        // remove semaphore
        if (sem_id >= 0){
            if (semctl(sem_id, 0, IPC_RMID, 0) >= 0){
                printf("Removed semaphore %d\n", sem_id);
            }else{
                perror("Failed to remove semaphore");
            }
            sem_id = -1;
        }
        printf("Parent process %d closed.\n", getpid());
    }else{
        printf("Child process %d closed.\n", getpid());
    }
    printf("\n");
}

// create socket
void create_socket(){

    // create socket
    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(-1);
    }
    int yes = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // bind socket
    if(bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(-1);
    }
    printf("Socket binded.\n");

    // listen
    if(listen(server_sockfd, NUM_CLIENT) < 0){
        perror("listen");
        exit(-1);
    }
    printf("Socket listening...\n");

     printf("server [%s:%d] --- ready\n", 
        inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
}

void create_shared_memory(){
    // Shared Memory
    if((shm_id = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666)) < 0){
        perror("shmget");
        exit(-1);
    }
    /* attach shared memory */ 
    if((shm_ptr = shmat(shm_id, NULL, 0)) == (shared_data_t*) -1){
        perror("shmat");
        exit(-1);
    }
}

void create_semaphore(){
    // Semaphore
    // create 2 semaphores
    int sem_num = 2;
    if((sem_id = semget(IPC_PRIVATE, sem_num, IPC_CREAT | 0666)) < 0){
        perror("semget");
        exit(-1);
    }
    // set semaphore value to 1
    unsigned short sem_val[sem_num];
    for(int i=0;i<sem_num;i++){
        sem_val[i] = 1;
    }
    if(semctl(sem_id, 0, SETALL, sem_val) < 0){
        perror("semctl");
        exit(-1);
    }
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

/* P () - returns 0 if OK; -1 if there was a problem */ 
int P (int s,int n)  { 
    struct sembuf sop;  /* the operation parameters */ 
    sop.sem_num =  n;   /* access the n-th sem in the array */ 
    sop.sem_op  = -1;   /* wait..*/ 
    sop.sem_flg =  0;   /* no special options needed */ 
    
    if (semop (s, &sop, 1) < 0) {  
        fprintf(stderr,"P(): semop failed: %s\n",strerror(errno)); 
        exit(-1); 
    } else { 
        return 0; 
    } 
}

/* V() - returns 0 if OK; -1 if there was a problem */ 
int V(int s, int n) { 
    struct sembuf sop; /* the operation parameters */ 
    sop.sem_num =  n;   /* access the n-th sem in the array */ 
    sop.sem_op  =  1; /* signal */ 
    sop.sem_flg =  0; /* no special options needed */ 
    
    if (semop(s, &sop, 1) < 0) {  
        fprintf(stderr,"V(): semop failed: %s\n",strerror(errno)); 
        exit(-1); 
    } else { 
        return 0; 
    } 
}