#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/select.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define OBJ_NUM 3

#define FOOD_INDEX 0
#define CONCERT_INDEX 1
#define ELECS_INDEX 2
#define RECORD_PATH "./bookingRecord"

static char* obj_names[OBJ_NUM] = {"Food", "Concert", "Electronics"};

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
    int stage;
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const unsigned char IAC_IP[3] = "\xff\xf4";

const char* EnterIdMessage = "Please enter your id (to check your booking state):\n";
const char* WRIdMessage = "[Error] Operation failed. Please try again.\n";
const char* LockMessage = "Locked.\n";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id;          // 902001-902020
    int bookingState[OBJ_NUM]; // 1 means booked, 0 means not.
}record;

int readLock[21] = {};
bool writeLock[21] = {};

int handle_read(request* reqP) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     */
    int r;
    char buf[512] = "";
    memset(buf, 0, sizeof(buf));

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                fprintf(stderr, "Client presses ctrl+C....\n");
                return 0;
            }
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

int validId(char* str){
    char* ptr; 
    int Id = strtol(str, &ptr, 0);
    if(Id >= 902001 && Id <= 902020 && strlen(str) == 6){
        return Id;
    }
    return -1;
}

int isint(char* str, bool* err){
    bool a = 1;
    bool b = 1;
    for(int i = 0; i < strlen(str); i++){
        if(str[i] < '0' || str[i] > '9'){
            a = 0;
        }
    }
    if(str[0] == '-' && str[1] != '\0'){
        for(int i = 1; i < strlen(str); i++){
            if(str[i] < '0' || str[i] > '9'){
                b = 0;
            }
        }
    }
    else{
        b = 0;
    }
    if(a || b){
        return strtol(str, NULL, 0);
    }
    else{
        *err = 1;
        return -1;
    }
}

int RDLockOn(int fd, int id){
    int sequ = id;
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_start = sizeof(record)*sequ;
    lock.l_whence = SEEK_SET;
    lock.l_len = sizeof(record);
    return fcntl(fd, F_SETLK, &lock);
}

int WRLockOn(int fd, int id){
    int sequ = id;
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_start = sizeof(record)*sequ;
    lock.l_whence = SEEK_SET;
    lock.l_len = sizeof(record);
    return fcntl(fd, F_SETLK, &lock);
}

int LockOff(int fd, int id){
    int sequ = id;
    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_start = sizeof(record)*sequ;
    lock.l_whence = SEEK_SET;
    lock.l_len = sizeof(record);
    return fcntl(fd, F_SETLK, &lock);
}


int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;
    int conn_fd_arr[maxfd];
    int fd_counter = 0;

    int conn_fd;  // fd for a new connection with client
    int file_fd = open(RECORD_PATH, O_RDWR);  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    fd_set master_set, working_set;
    FD_ZERO( &master_set );
    FD_SET( svr.listen_fd, &master_set);
    struct timeval time_out;
    time_out.tv_sec = 0;
    time_out.tv_usec = 1;


    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    while (1) {
        // TODO: Add IO multiplexing
        memcpy(&working_set, &master_set, sizeof(master_set));
        // Check new connection
        select(maxfd, &working_set, NULL, NULL, &time_out);
        if(FD_ISSET(svr.listen_fd, &working_set)){
            clilen = sizeof(cliaddr);
            conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    continue;
                }
                ERR_EXIT("accept");
            }

            requestP[conn_fd].conn_fd = conn_fd;
            strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);

            sprintf(buf, "Please enter your id (to check your booking state):\n");
            write(requestP[conn_fd].conn_fd, buf, strlen(buf));
            requestP[conn_fd].stage = 0;
            FD_SET(conn_fd, &master_set);
        }
        
        char* buffer = requestP[conn_fd].buf;
        
    // TODO: handle requests from clients
    #ifdef READ_SERVER  
        for(int i = 3; i < maxfd; i++){
            conn_fd = i; 
            if(conn_fd == svr.listen_fd || conn_fd == file_fd){
                continue;
            }
            if(FD_ISSET(conn_fd, &working_set) && requestP[conn_fd].stage == 0){
                int ret = handle_read(&requestP[conn_fd]);
                if(ret <= 0){
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                }
                int currentId;
                if((currentId = validId(requestP[conn_fd].buf)) > 0){
                    currentId -= 902001;
                    if(writeLock[currentId] == 0 && RDLockOn(file_fd, currentId) != -1){
                        readLock[currentId]++;
                        lseek(file_fd, sizeof(record)*currentId, SEEK_SET);
                        int read_buf[4];
                        read(file_fd, read_buf, sizeof(record));
                        sprintf(buf, "Food: %d booked\nConcert: %d booked\nElectronics: %d booked\n\n(Type Exit to leave...)\n", read_buf[1], read_buf[2], read_buf[3]);
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        requestP[conn_fd].stage = 1;
                        requestP[conn_fd].id = currentId;
                    }
                    else{
                        sprintf(buf, "Locked.\n");
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);
                    }
                }
                else if(requestP[conn_fd].stage == 0){
                    sprintf(buf, "[Error] Operation failed. Please try again.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                }
            }
            else if(FD_ISSET(conn_fd, &working_set) && requestP[conn_fd].stage == 1){
                int ret = handle_read(&requestP[conn_fd]);
                if(ret <= 0 || strcmp(requestP[conn_fd].buf, "Exit") == 0){
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    close(requestP[conn_fd].conn_fd);
                    int curr_id = requestP[conn_fd].id;
                    free_request(&requestP[conn_fd]);
                    readLock[curr_id]--;
                    if(readLock[curr_id] == 0){
                        LockOff(file_fd, curr_id);
                    }
                }
            }
        }
    #elif defined WRITE_SERVER
        for(int i = 3; i < maxfd; i++){
            conn_fd = i;
            if(conn_fd == svr.listen_fd || conn_fd == file_fd){
                continue;
            }
            if(FD_ISSET(conn_fd, &working_set) && requestP[conn_fd].stage == 0){
                int ret = handle_read(&requestP[conn_fd]);
                int currentId;
                if(ret <= 0){
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                }
                else if((currentId = validId(requestP[conn_fd].buf)) > 0){
                    currentId -= 902001;
                    if(writeLock[currentId] == 0 && readLock[currentId] == 0 && WRLockOn(file_fd, currentId) != -1){
                        writeLock[currentId] = 1;
                        lseek(file_fd, sizeof(record)*currentId, SEEK_SET);
                        int read_buf[4];
                        read(file_fd, read_buf, sizeof(record));
                        sprintf(buf, "Food: %d booked\nConcert: %d booked\nElectronics: %d booked\n\nPlease input your booking command. (Food, Concert, Electronics. Positive/negative value increases/decreases the booking amount.):\n", read_buf[1], read_buf[2], read_buf[3]);
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        requestP[conn_fd].stage = 1;
                        requestP[conn_fd].id = currentId;
                    }    
                    else{
                        sprintf(buf, "Locked.\n");
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);
                    }
                }
                else{
                    sprintf(buf, "[Error] Operation failed. Please try again.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                }
            }
            else if(FD_ISSET(conn_fd, &working_set) && requestP[conn_fd].stage == 1){
                int retval = handle_read(&requestP[conn_fd]);
                if(retval <= 0){
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    break;
                }
                int target = requestP[conn_fd].id;
                lseek(file_fd, sizeof(record)*target, SEEK_SET);
                int read_buf[4];
                read(file_fd, read_buf, sizeof(record));
                char* food;
                char* concert;
                char* electronic;
                food = strtok(requestP[conn_fd].buf, " ");
                concert = strtok(NULL, " ");
                electronic = strtok(NULL, " ");
                bool err = 0;
                int food_num = isint(food, &err);
                if(err){
                    sprintf(buf, "[Error] Operation failed. Please try again.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
                err = 0;
                int concert_num = isint(concert, &err);
                if(err){
                    sprintf(buf, "[Error] Operation failed. Please try again.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
                err = 0;
                int electronic_num = isint(electronic, &err);
                if(err){
                    sprintf(buf, "[Error] Operation failed. Please try again.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
                err = 0;
                int cur_total = read_buf[1] + read_buf[2] + read_buf[3];
                if(cur_total + food_num + concert_num + electronic_num > 15){
                    sprintf(buf, "[Error] Sorry, but you cannot book more than 15 items in total.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
                else if(food_num + read_buf[1] < 0){
                    sprintf(buf, "[Error] Sorry, but you cannot book less than 0 items.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
                else if(concert_num + read_buf[2] < 0){
                    sprintf(buf, "[Error] Sorry, but you cannot book less than 0 items.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
                else if(electronic_num + read_buf[3] < 0){
                    sprintf(buf, "[Error] Sorry, but you cannot book less than 0 items.\n");
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
                else{
                    read_buf[1] += food_num;
                    read_buf[2] += concert_num;
                    read_buf[3] += electronic_num;
                    lseek(file_fd, sizeof(record)*requestP[conn_fd].id, SEEK_SET);
                    write(file_fd, read_buf, sizeof(int)*4);
                    sprintf(buf, "Bookings for user %d are updated, the new booking state is:\nFood: %d booked\nConcert: %d booked\nElectronics: %d booked\n", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    FD_CLR(requestP[conn_fd].conn_fd, &master_set);
                    int curr_id = requestP[conn_fd].id;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    writeLock[curr_id] = 0;
                    LockOff(file_fd, curr_id);
                    continue;
                }
            }
        }
    #endif
    }
    close(file_fd);
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP->stage = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
