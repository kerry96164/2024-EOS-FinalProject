/*
To-do list

1. semaphore to implement CS (V)
2. socket to add node(V)
3. time (V)
4. timer 
5. socket to transmit ranking board (V)
6. add system("clear")
7. make sure no zombie process (v)
8. sigaction make sure close add socket fds (shmctl failed: Invalid argument) (v)
9. check if signal works properly(delete sem and shm) (v)
10. server準備畫面如game strat game over等 (其實不用)
11. client 用system clear把畫面弄乾淨一點(v)
12. father run detect_ball.py with select()
*/

# include <wiringPi.h>
# include <time.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/ipc.h>
# include <sys/shm.h>
# include <unistd.h>
# include <sys/sem.h> // for semaphore
# include <signal.h>
# include <errno.h>
# include <sys/socket.h> // for socket
# include <netinet/in.h>  // sockaddr_in, htons, INADDR_ANY
# include <arpa/inet.h>   // htons, inet_ntoa
# include <sys/wait.h> // waitpid()
# include <sys/time.h> 


# define SHM_SIZE 1024
# define NAME_SIZE 10
# define CAPACITY 10
# define SEM_MODE 666
# define BUFFERSIZE 1024
# define SEM_KEY 1122334455
# define SHM_KEY 11223344
# define SHM_KEY2 1122334 // for is_running value
# define GAMETIME 80

int shmid;
int shmid2;
int server_fd;
int* is_running;
int game_score;
int P(int s);
int V(int s);

/*-------------------------------------------*/
# define CLK 27  // CLK 對應 GPIO27
# define DIO 17  // DIO 對應 GPIO17

// 定義 TM1637 的指令碼
# define TM1637_CMD1 0x40
# define TM1637_CMD2 0xC0
# define TM1637_CMD3 0x88
void parent_func();

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
/*-------------------------------------------*/




typedef struct Node{
    char name[NAME_SIZE];
    int score;
    char time[20];
    int offset; // offset from first NODE
}NODE;

typedef struct list{
    int head;
    int size; // node amount
    NODE nodes[CAPACITY]; // array
}LIST;
LIST *list;

void get_current_time(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M", t);
}


void countdown_timer(int seconds) {

    int s = semget(SEM_KEY, 1, 0);
    if (s < 0) {
        printf("cannot find semaphore\n");
    }
    while (seconds > 0) {
        system("clear");

        printf("GAME TIME LEFT：%02d:%02d\n", seconds / 60, seconds % 60);
        TM1637_displayTime(seconds);
        parent_func();
        sleep(1); 
        seconds--; 
    }

    // 倒數結束，顯示 0000
    uint8_t zeroDisplay[] = {0, 0, 0, 0};
    TM1637_displayDigits(zeroDisplay);

    system("clear");
    printf("GAME OVER！Login to start a new game...\n");

    V(s);
    V(s);
    usleep(5000);
    (*is_running) = 0;
}


void SIGINT_handler(int signum) { 

    // remove semaphore
    int s = semget(SEM_KEY, 1, 0);
    if (s >= 0) {
        semctl(s, 0, IPC_RMID, 0);
    }

    // detach shared memory
    if (shmdt(list) == -1) {
        perror("shmdt");
        exit(1);
    }
    
    // remove shared memory
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        perror("shmctl failed");
        exit(EXIT_FAILURE);
    }

    // detach shared memory2
    if (shmdt(is_running) == -1) {
        perror("shmdt2");
        exit(1);
    }
    
    // remove shared memory
    if (shmctl(shmid2, IPC_RMID, NULL) < 0) {
        perror("shmctl failed");
        exit(EXIT_FAILURE);
    }
    /*close socket*/

    close(server_fd);
    exit(0); 
}

/* Use for killing Zombie process */
void zombie_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* P () - returns 0 if OK; -1 if there was a problem */
/* for acquire (-1) */
int P(int s){

    struct sembuf sop; /* the operation parameters */
    sop.sem_num = 0;   /* access the 1st (and only) sem in the array */
    sop.sem_op = -1;   /* wait, acquire */
    sop.sem_flg = 0;   /* no special options needed */

    if (semop(s, &sop, 1) < 0) { // semaphore operation to acquire
        fprintf(stderr, "P(): semop failed: %s\n", strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

/* V() - returns 0 if OK; -1 if there was a problem */
int V(int s) {

    struct sembuf sop; /* the operation parameters */
    sop.sem_num = 0;   /* the 1st (and only) sem in the array */
    sop.sem_op = 1;    /* signal */
    sop.sem_flg = 0;   /* no special options needed */

    if (semop(s, &sop, 1) < 0) {
        fprintf(stderr, "V(): semop failed: %s\n", strerror(errno));
        return -1;
    } else {
        return 0;
    }
}


void add(LIST *list, char* name, int score){


    if (list->size >= CAPACITY){
        printf("Ranking board is full\n");
        return;
    }

    int new_index = list->size++; // assign then ++
    strcpy(list->nodes[new_index].name, name);
    get_current_time(list->nodes[new_index].time, sizeof(list->nodes[new_index].time)); 
    //strcpy(list->nodes[new_index].time, name);
    list->nodes[new_index].score = score;
    list->nodes[new_index].offset = -1;

    if (list->head == -1){
        list->head = new_index;
    }else{
        // insertion sort
        int current = list-> head;
        int previous = -1;
        while(current != -1 && score < list->nodes[current].score){
            previous = current;
            current = list->nodes[current].offset;
        }
        if(previous == -1) { 
            list->nodes[new_index].offset = list->head;
            list->head = new_index;
        } else {
            // insert in middle
            list->nodes[previous].offset = new_index;
            list->nodes[new_index].offset = current;
        }
    }

}


void print_list(LIST* list, int client_fd) {
    int current = list->head;
    char send_buf[BUFFERSIZE];
    int index = 0;

    // add title
    snprintf(send_buf + index, BUFFERSIZE - index, "==========Ranking Board==========\n");
    index = strlen(send_buf);

    // sprintf into send_buf
    while (current != -1) {
        int n = snprintf(send_buf + index, BUFFERSIZE - index, 
                         "%-13s %-10d %-13s\n", 
                         list->nodes[current].name, 
                         list->nodes[current].score, 
                         list->nodes[current].time);

        if (n < 0) {
            perror("Error formatting ranking data");
            return;
        }

        index += n;
        if (index >= BUFFERSIZE - 1) {
            // if buffer is full
            break;
        }

        current = list->nodes[current].offset;
    }

    if (send(client_fd, send_buf, index, 0) == -1) {
        perror("Failed to send ranking board");
    }
}

void child_func(int client_fd){
    char recv_buf[BUFFERSIZE] = {0};
    char name[NAME_SIZE];
    char choice_from_client[10] = {0};
    char send_buf[BUFFERSIZE] = {0};
    int score;
    int s = semget(SEM_KEY, 1, 0);
    if (s < 0) {
        printf("cannot find semaphore\n");
    }

    while(1){
        memset(recv_buf, 0 , BUFFERSIZE);
        if (recv(client_fd, recv_buf, BUFFERSIZE, 0) > 0) {
        
            sscanf(recv_buf, "%s %s", choice_from_client, name);
            if (strncmp(choice_from_client, "first", 5) == 0){
                printf("1\n");
                if (*is_running == 0){
                    (*is_running)=1;
                    countdown_timer(GAMETIME);
                }
                P(s);
                FILE *file = fopen("game_score.txt", "r");
                if (file == NULL) {
                    perror("fopen");
                }
                fscanf(file, "%d", &game_score);
                fclose(file);
                add(list, name, game_score);
            }else if(strncmp(choice_from_client, "second", 7)== 0){

                printf("2\n");
                print_list(list, client_fd);
            }else if(strncmp(choice_from_client, "exit", 4)== 0){

                printf("3\n");
                // detach shared memory
                if (shmdt(list) == -1) {
                    perror("shmdt");
                    exit(1);
                }
                // detach shared memory2
                if (shmdt(is_running) == -1) {
                    perror("shmdt2");
                    exit(1);
                }
    

            }
        }
    }
    exit(0);
}

void parent_func(){
    
    pid_t pid = fork();
    if (pid == 0){
       execlp("python3", "python3", "stream.py", (char *)NULL);
    }
    
}

int main(int argc, char* argv[]) {        

    /*--- child process handler ---*/
    signal(SIGCHLD, zombie_handler);

    int s; // for shmaphore id


    if (argc != 2){
        printf("Usage: ./ranking <port>\n");
        exit(EXIT_FAILURE);
    }
    
    // Create a binary semaphore 
    s = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | SEM_MODE);
    if (s < 0){
        perror("Semaphore create failed");
        exit(EXIT_FAILURE);
    }

    // Set semaphore initial value = 0 for sync when countdown to 0 and each task read score from txt
    int val = 0;
    if (semctl(s, 0, SETVAL, val) < 0){
        perror("Semaphore set value to 1 failed");
        exit(EXIT_FAILURE);
    }

    // shm for recording timer value
    shmid2 = shmget(SHM_KEY2, sizeof(int), 0666 | IPC_CREAT); 

    if (shmid2 == -1) {
        perror("shmget2");
        exit(1);
    }
    
    is_running = (int* )shmat(shmid2, NULL, 0);
    if (is_running == (int *)-1) {
        perror("shmat2");
        exit(1);
    }
    *is_running = 0; // not running

    // shm for ranking board
    shmid = shmget(SHM_KEY, SHM_SIZE, 0666 | IPC_CREAT); 

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    list = (LIST *)shmat(shmid, NULL, 0);
    if (list == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // initialize the list
    list->head = -1;
    list->size = 0;

    // setting sockaddr_in 
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int port = atoi(argv[1]);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // force using socket address already in use 
    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // AF_INET = IPv4
    // SOCK_STREAM = TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // start binding
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Server binding failed");
        exit(EXIT_FAILURE);
    }

    // start listening with maximum 5 client connecting
    if (listen(server_fd, 5) < 0) {
        perror("Server listening failed");
        exit(EXIT_FAILURE);
    }
    //printf("Server is now listening...\n");

    // sigaction for SIGINT to close(serverfd) and remove sem, shm
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); 
    sa.sa_handler = &SIGINT_handler; 
    sigaction(SIGINT, &sa, NULL);

    // initializing wiringPi
    if (wiringPiSetupGpio() == -1) {
        printf("Setup wiringPi failed!");
        return -1;
    }

    pinMode(CLK, OUTPUT);
    pinMode(DIO, OUTPUT);

    // setup brightness (0x00 to 0x07)
    TM1637_setBrightness(0x07);
    printf("GAME START!!!\n");
    //parent_func();

    
    while(1){
        
        int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_fd < 0) {
            perror("Accepting client failed");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            close(client_fd);
            continue;
        } else if (pid == 0) { 
            close(server_fd);
            child_func(client_fd);
            close(client_fd);
            exit(0);
        } else { 
            close(client_fd);
        }
        

    }

    return 0;

}
