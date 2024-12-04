# include <stdio.h>
# include <unistd.h> // for sleep() fork() exec() read() close() lseek() dup() ...
# include <stdlib.h>
# include <netinet/in.h>  // sockaddr_in, htons, INADDR_ANY
# include <arpa/inet.h>   // htons, inet_ntoa
# include <sys/socket.h> // for socket
# include <string.h>
# include <errno.h>
# include <sys/ipc.h>
# include <sys/shm.h>
# include <sys/sem.h>
# include <termios.h> // change input mode
# include <sys/ioctl.h>


# define RED     "\033[31m"
# define GREEN   "\033[32m"
# define YELLOW  "\033[33m"
# define BLUE    "\033[34m"
# define MAGENTA "\033[35m"
# define CYAN    "\033[36m"
# define RESET   "\033[0m"

# define NAME_SIZE 10
# define BUFFERSIZE 1024
# define GAMETIME 10
static struct termios stored_settings;

void print_centered(const char *text) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); 
    int width = w.ws_col; 
    int len = strlen(text);
    int padding = (width - len) / 2;
    for (int i = 0; i < padding; i++) {
        printf(" ");
    }
    printf("%s\n", text);
}

void print_frame( char *text) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); 
    int width = w.ws_col;
    int text_len = strlen(text);
    int padding = (width - text_len - 4) / 2; 

    for (int i = 0; i < width; i++) {
        printf("*");
    }
    printf("\n");

    printf("**");
    for (int i = 0; i < padding; i++) {
        printf(" ");
    }
    printf("%s", text);
    for (int i = 0; i < padding; i++) {
        printf(" ");
    }
    if ((text_len + 4) % 2 != 0) {
        printf(" ");
    }
    printf("**\n");

    for (int i = 0; i < width; i++) {
        printf("*");
    }
    printf("\n");
}



void print_large_text(const char *text, const char *color) {
    printf("%s", color);
    printf("  ██████   █████  ███    ███ ███████       ██████  ████████  █████   ██████  ████████ \n");
    printf(" ██       ██   ██ ████  ████ ██           ██          ██    ██   ██  ██   ██    ██    \n");
    printf(" ██   ███ ███████ ██ ████ ██ █████         ██████     ██    ███████  ██████     ██    \n");
    printf(" ██    ██ ██   ██ ██  ██  ██ ██                 ██    ██    ██   ██  ██   ██    ██    \n");
    printf("  ██████  ██   ██ ██      ██ ███████       ██████     ██    ██   ██  ██   ██    ██    \n");
    printf(RESET); 
}

void set_keypress (void){
    struct termios new_settings;

    tcgetattr (0, &stored_settings);

    new_settings = stored_settings;

    /* Disable canonical mode, and set buffer size to 1 byte */
    new_settings.c_lflag &= (~ICANON);
    new_settings.c_cc[VTIME] = 0;
    new_settings.c_cc[VMIN] = 1;

    tcsetattr (0, TCSANOW, &new_settings);
    return;
}

void reset_keypress (void){
    tcsetattr (0, TCSANOW, &stored_settings);
    return;
}



int main(int argc, char* argv[]){
    char send_buf[50] = {0};
    char recv_buf[BUFFERSIZE] = {0};

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

    while(1){
        system("clear");
        print_frame("==== GAME MENU ====");
        print_centered("1. User Login and Start Game");
        print_centered("2. Ranking Board");
        print_centered("3. Exit");
        print_centered("Enter Your Choice: ");

        
        int choice;
        char user_name[NAME_SIZE];
        scanf("%d", &choice);
        if (choice == 1) {
            system("clear");
            print_centered("Please enter your name:");
            scanf("%10s", user_name);
            sprintf(send_buf, "%s %s %s", "first", user_name, "10");
            send(server_fd, send_buf, 50, 0);
            memset(send_buf, 0, sizeof(send_buf));
            system("clear");
            print_large_text("GAME START!!!", YELLOW);
            sleep(GAMETIME);

        } else if (choice == 2) {
            send(server_fd, "second", 7, 0);
            usleep(50000);
            system("clear");
            if (recv(server_fd, recv_buf, BUFFERSIZE, 0) > 0){
                printf("%s", recv_buf);
                set_keypress();
                printf("<!-- Press any button -->\n");
                getchar(); // Use getchar to pause
                getchar();
                reset_keypress();
                system("clear");
            }
        } else if(choice == 3){
            close(server_fd);
            system("clear");
            break;

        }
    }

    return 0;


}