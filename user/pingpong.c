#include "kernel/types.h"
#include "user.h"

int main(int argc,char* argv[]){
    int pipe_p2c[2]; // pipe from parent to child
    int pipe_c2p[2]; // pipe from child to parent
    int pipe_res1 = pipe(pipe_p2c);
    int pipe_res2 = pipe(pipe_c2p);
    if(pipe_res1) {
        printf("create pipe p2c failed!\n");
        exit(pipe_res1);
    }
    if(pipe_res2) {
        printf("create pipe c2p failed!\n");
        exit(pipe_res2);
    }

    int pid = fork();
    if(pid < 0) {
        printf("fork failed!\n");
        exit(pid);
    }
    char buf[10];
    if(pid) {
        // main process
        close(pipe_p2c[0]); // close read end of p2c
        close(pipe_c2p[1]); // close write end of c2p
        write(pipe_p2c[1], "ping", 4);
        read(pipe_c2p[0], buf, 4);
        if (strcmp(buf, "pong") == 0) {
            printf("%d: received pong\n", getpid());
        } else {
            printf("ERROR: %d: received %s\n", getpid(), buf);
            exit(-1);
        }
        close(pipe_p2c[1]);
        close(pipe_c2p[0]);
    } else {
        // child process
        close(pipe_p2c[1]); // close write end of p2c
        close(pipe_c2p[0]); // close read end of c2p
        read(pipe_p2c[0], buf, 4);
        if (strcmp(buf, "ping") == 0) {
            printf("%d: received ping\n", pid);
        } else {
            printf("ERROR: %d: received %s\n", pid, buf);
            exit(-1);
        }
        write(pipe_c2p[1], "pong", 4);
        close(pipe_p2c[0]);
        close(pipe_c2p[1]);
    }
    exit(0);
}