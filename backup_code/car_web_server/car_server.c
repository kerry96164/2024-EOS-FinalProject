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

#define PORT 50143
#define NUM_CLIENT 5
#define DEVICE "/dev/car_device"

typedef struct {
    int right;
    int left;
} wheel_speed_t;

int shm_size = sizeof(wheel_speed_t);
wheel_speed_t *shm_ptr; // shared memory pointer (used for speed)

int server_sockfd = -1;
int reply_sockfd = -1;
struct sockaddr_in server_addr, client_addr;
int client_len;
int shm_id = -1;
int sem_id = -1;
pid_t webcam_pid = -1;
pid_t child_pid = -1;

void close_handler();
void sigint_handler(int signum);
void zombie_handler(int signum);
void create_socket();
void create_shared_memory();
void create_semaphore();
void update_speed_from_socket();
void motor_controler();
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
        execl("/usr/bin/python3", "python3", "webcam_server.py", NULL);
        perror("execl");
        exit(-1);
    }
    create_socket();

    while (1){
        reply_sockfd = accept(server_sockfd, &client_addr, &client_len);
        if (reply_sockfd < 0) {
            perror("accept");
            exit(-1);
        }
        printf("client [%s:%d] --- connect\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        if(child_pid = fork() == 0){
            // child process
            close(server_sockfd);
            update_speed_from_socket();
            exit(-1);
        }
    }
    motor_controler();

    return 0;
}

// SIGINT: Ctrl-C
void sigint_handler(int signum) {
    printf("\n Received interrupt signal (Ctrl-C). \nExiting program safely...\n");
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

    // if(child_pid&timer_pid){ // parent process
    //     // remove shared memory
    //     if (shm_id >= 0){
    //         if(shmctl(shm_id, IPC_RMID, NULL) >=0 ){
    //             printf("Removed shared memory %d\n", shm_id);
    //         }else{
    //             perror("Failed to remove shared memory");
    //         }
    //         shm_id = -1;
    //     }
    //     // remove semaphore
    //     if (sem_id >= 0){
    //         if (semctl(sem_id, 0, IPC_RMID, 0) >= 0){
    //             printf("Removed semaphore %d\n", sem_id);
    //         }else{
    //             perror("Failed to remove semaphore");
    //         }
    //         sem_id = -1;
    //     }
    //     printf("Parent process %d closed.\n", getpid());
    // }else{
    //     printf("Child process %d closed.\n", getpid());
    // }
    // printf("\n");
}

// create socket
void create_socket(){

    // create socket
    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(-1);
    }

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
    if((shm_ptr = shmat(shm_id, NULL, 0)) == (int*) -1){
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
    if(semctl(sem_id, 0, SETALL, 1) < 0){
        perror("semctl");
        exit(-1);
    }
}

void motor_controler(){
    int fd;
    if((fd = open(DEVICE, O_RDWR)) < 0){
        perror("open");
        exit(-1);
    }
    while(1){
        P(sem_id, 0);
        P(sem_id, 1);
        ioctl(fd, 0, shm_ptr->linear);
        ioctl(fd, 1, shm_ptr->angular);
        V(sem_id, 0);
        V(sem_id, 1);
    }
    close(fd);
}

void update_speed_from_socket(){
    wheel_speed_t pri_var;
    char buffer[256];
    char ch;
    while(1){
        if(read(reply_sockfd, buffer, sizeof(buffer)) < 0){
            perror("read");
            exit(-1);
        }
        sscanf(buffer, "%s", ch);
        switch (ch){
            case 'w':
            case 'W':
                pri_var.left = 1;
                pri_var.right = 1;
                break;
            case 's':
            case 'S':
                pri_var.left = -1;
                pri_var.right = -1;
                break;
            case 'a':
            case 'A':
                pri_var.left = 0;
                pri_var.right = 1;
                break;
            case 'd':
            case 'D':
                pri_var.left = 1;
                pri_var.right = 0;
                break;
            case 'q':
            case 'Q':
                printf("Exiting ...\n");
                exit(0);
                break;
            default:
                break;
        }
        // write to shared memory
        if(pri_var.left != 0){
            P(sem_id, 0);
            if(shm_ptr->left + pri_var.left > 3){
                shm_ptr->left = 3;
            }else if(shm_ptr->left + pri_var.left < -3){
                shm_ptr->left = -3;
            }else{
                shm_ptr->left +=  pri_var.left;
            }
            V(sem_id, 0);
        }
        if(pri_var.right != 0){
            P(sem_id, 1);
            if(shm_ptr->right + pri_var.right > 3){
                shm_ptr->right = 3;
            }else if(shm_ptr->right + pri_var.right < -3){
                shm_ptr->right = -3;
            }else{
                shm_ptr->right +=  pri_var.right;
            }
            V(sem_id, 1);
        }
        //printf("Received speed: linear=%d, angular=%d\n", speed.linear, speed.angular);
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
        return -1; 
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
        return -1; 
    } else { 
        return 0; 
    } 
}