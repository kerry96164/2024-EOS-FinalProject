# include <stdio.h>
# include <unistd.h> // for fork() exec() read() close() lseek() dup() ...
# include <stdlib.h>
# include <netinet/in.h>  // sockaddr_in, htons, INADDR_ANY
# include <arpa/inet.h>   // htons, inet_ntoa
# include <sys/socket.h> // for socket
# include <string.h>
# include <errno.h>
# include <sys/ipc.h>
# include <sys/shm.h>
# include <sys/sem.h>


#define SHM_SIZE 1024
#define NAME_SIZE 10
#define CAPACITY 10
#define SEM_KEY 1122334455
#define SHM_KEY 11223344


typedef struct Node {
    char name[NAME_SIZE];
    int score;
    char time[20];
    int offset;
} NODE;

typedef struct list {
    int head;
    int size;
    NODE nodes[CAPACITY];
} LIST;
LIST* list;

// Print the ranking board
void print_list(LIST* list) {
    int current = list->head;
    printf("==========Ranking Board==========\n");
    while (current != -1) {
        printf("%-13s %-10d %-13s\n", list->nodes[current].name, list->nodes[current].score, list->nodes[current].time);
        current = list->nodes[current].offset;
    }
}


int main(int argc, char* argv[]){
    char send_buf[50] = {0};

    if (argc != 3){
        printf("Usage: ./ranking_client <ip> <port>\n");
        exit(EXIT_FAILURE);
    }

    int server_fd = 0;
    server_fd =  socket(AF_INET , SOCK_STREAM , 0);
    if(server_fd == -1){
        perror("Connect to server failed");
    }

    int port = atoi(argv[2]);
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(argv[1]);
    address.sin_port = htons(port);

    if(connect(server_fd,(struct sockaddr *)&address, addrlen) < 0){
        printf("Connection error\n");
    }

    int shmid = shmget(SHM_KEY, SHM_SIZE, 0666); // Use the same key and size as the server
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    LIST* list = (LIST*)shmat(shmid, NULL, 0);
    if (list == (void*)-1) {
        perror("shmat failed");
        exit(1);
    }

    while(1){
        printf("==== MENU ====\n");
        printf("1. User Login and Start game\n");
        printf("2. Ranking Board\n");
        printf("3. exit\n");
        printf("Enter Your Choice: ");
        int choice;
        char user_name[NAME_SIZE];
        scanf("%d", &choice);
        if (choice == 1) {
            printf("Please enter your name:");
            scanf("%10s", user_name);
            sprintf(send_buf, "%s %s", user_name, "10");
            send(server_fd, send_buf, 50, 0);
            memset(send_buf, 0, sizeof(send_buf));

        } else if (choice == 2) {
            print_list(list);
        } else if(choice == 3){
            close(server_fd);
            if (shmdt(list) == -1) {
                perror("shmdt failed");
            }
            break;

        }
    }

    return 0;


}