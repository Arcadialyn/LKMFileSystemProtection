#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#define NETLINK_TEST 22
#define MAX_PAYLOAD 1024  // maximum payload size

#define PROC_DIR "/proc/gsfileprotection"
#define PROC_PROTECT "/proc/gsfileprotection/protect"
#define PROC_CTRL "/proc/gsfileprotection/ctrl"
#define PROC_NOTI "/proc/gsfileprotection/noti"

#define FILEPATH_SIZE 256
#define FILELIST_SIZE 256
#define BUFFER_SIZE (FILEPATH_SIZE * FILELIST_SIZE)

int protectfd;
int ctrlfd;
int notifd;

struct fileList {
    char filePath[FILEPATH_SIZE];
    char type;
    struct fileList *next;
} *fileList_root;

struct fileList *tail;

void refreshstdin() {
    char ch;
    while (ch = getchar()) {
        if (ch != '\n');
        break;
    }
}

int notificationReceiver() {
    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr *nlh = NULL;
    struct iovec iov;
    struct msghdr msg;
    int sock_fd, retval;
    
    // Create a socket
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_TEST);
    if(sock_fd == -1){
        printf("error getting socket: %s", strerror(errno));
        return -1;
    }
    
    // To prepare binding
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK; 
    src_addr.nl_pid = getpid();  // self pid 
    src_addr.nl_groups = 1; // multi cast
    
    retval = bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(retval < 0){
        printf("bind failed: %s\n", strerror(errno));
        close(sock_fd);
        return -1;
    }
    
    // To prepare recvmsg
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if(!nlh){
        printf("malloc nlmsghdr error!\n");
        close(sock_fd);
        return -1;
    }
    
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    iov.iov_base = (void *)nlh;
    iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);
    
    memset(&msg, 0, sizeof(msg));
    memset(&dest_addr, 0, sizeof(dest_addr));
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    // Read message from kernel
    while(1){
        recvmsg(sock_fd, &msg, 0);
        printf("Received message: %s\n", (char *)NLMSG_DATA(nlh));
    }
    
    close(sock_fd);
    
    return 0;
}

void printFileList() {
    struct fileList *next = fileList_root;
    while (next) {
        printf("%s@%c\n", next->filePath, next->type);
        next = next->next;
    }
}

void refreshFileList() {
    char *buffer;
    char *p;
    int len = 0;
    struct fileList *next = fileList_root;
    int tmp;
    
    buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
    
    p = buffer;
    while (next != 0) {
        tmp = strlen(next->filePath);
        
        strncpy(p, next->filePath, tmp);
        
        p += tmp;
        *p = '@';
        p++;
        *p = next->type;
        p++;
        *p = '\n';
        p++;
        
        next = next->next;
    }

    printf("buffer is:\n%s----\n", buffer);
    
    write(protectfd, buffer, strlen(buffer));
    
    free(buffer);
}

void addFile() {
    struct fileList *pfile = NULL;
    
    pfile = (struct fileList *)malloc(sizeof(struct fileList));
    memset(pfile, 0, sizeof(struct fileList));
    
    refreshstdin();
    printf("Input file path:\n");
    scanf("%[^\n]", pfile->filePath);
    
    printf("Input protect type: r(read), w(write), h(hide), d(delete)\n");
    refreshstdin();
    scanf("%c", &pfile->type);
    
    pfile->next = NULL;
    
    //printf("%s@%c\n----\n", pfile->filePath, pfile->type);
    
    if (tail == NULL) {
        fileList_root = pfile;
        tail = fileList_root;
    }
    else {
        tail->next = pfile;
        tail = tail->next;
    }
    
    refreshFileList();
}

void printmenu() {
    printf("1. Protect list\n");
    printf("2. Add\n");
    printf("3. Remove\n");
    printf("9. Quit\n");
}

void exitall() {
    printf("all terminate\n");
    close(protectfd);
    close(ctrlfd);
    close(notifd);
}

int main(int argc, char *argv[]) {
    pid_t pid;
    int command;
    int flag = 1;
        
    pid = fork();
    
    if (pid < 0) {
        printf("fork error\n");
        return -1;
    }
    else if (pid == 0) {
        // child
        notificationReceiver();
    }
    else {
        // father
        fileList_root = NULL;
        tail = fileList_root;
        
        protectfd = open(PROC_PROTECT, O_RDWR);
        ctrlfd = open(PROC_NOTI, O_RDWR);
        notifd = open(PROC_NOTI, O_RDWR);
        
        if (protectfd < 0 || ctrlfd < 0 || notifd < 0) {
            printf("open file error!\n");
            exitall();
            return -1;
        }

        printmenu();
        while (flag) {
            scanf("%d", &command);
            switch (command) {
                case 1:
                    printFileList();
                    break;
                case 2:
                    addFile();
                    break;
                case 3:
                    
                    break;
                case 9:
                    if (kill(pid, SIGTERM) < 0) {
                        printf("kill error\n");
                    }
                    else {
                        exitall();
                        flag = 0;
                    }
                    break;
                case 4:
                    
                    break;
                default:
                    printmenu();
                    break;
            }
        }
    }
}