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
10. server準備畫面如game strat game over等
11. client 用system clear把畫面弄乾淨一點
12. father run detect_ball.py with select()
*/

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
# define SHM_KEY2 1122334 // for timer value
# define GAMETIME 10

int shmid;
int shmid2;
int server_fd;
int* is_running;


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
    while (seconds > 0) {
        system("clear");

        printf("GAME TIME LEFT：%02d:%02d\n", seconds / 60, seconds % 60);
        sleep(1); 
        seconds--; 
    }

    system("clear");
    printf("GAME OVER！Login to start a new game...\n");
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

    int s = semget(SEM_KEY, 1, 0);

    P(s);
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
    V(s);

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

    while(1){
        memset(recv_buf, 0 , BUFFERSIZE);
        if (recv(client_fd, recv_buf, BUFFERSIZE, 0) > 0) {
        
            sscanf(recv_buf, "%s %s %d", choice_from_client, name, &score);
            if (strncmp(choice_from_client, "first", 5) == 0){
                printf("1\n");
                if (*is_running == 0){
                    (*is_running)=1;
                    countdown_timer(GAMETIME);
                }

                add(list, name, score);
            }else if(strncmp(choice_from_client, "second", 7)== 0){
                printf("2\n");
                print_list(list, client_fd);
            }
        }
    }
    exit(0);
}

void parent_func(){
    
    /*---execlp to call detect_ball.py----*/
    /*---use select to handle new conneciton if there's more connection---*/
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

    // Set semaphore initial value = 1 
    int val = 1;
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


    printf("GAME START!!!\n");

    while(1){
        int client_fd =accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd < 0){
            perror("Accepting client failed");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid < 0){
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }else if(pid == 0){
            close(server_fd);
            child_func(client_fd);
            close(client_fd);
            exit(0);
        }else{       
            close(client_fd);
        }
    }

    return 0;

}