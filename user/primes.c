#include "kernel/types.h"
#include "user.h"

#define assert(cond) do { if(!(cond)) { printf("assert failed at %s:%d\n", __FILE__, __LINE__); exit(-1); } } while(0)

#define END 35
#define NUM_PROCESS 11

int main()
{
    int pipes[NUM_PROCESS][2];
    int id = -1;
    for(int i=0; i < NUM_PROCESS; ++i) {
        int res = pipe(pipes[i]);
        if(res) {
            printf("create pipe failed!\n");
            exit(res);
        }
        if(fork()) {
            close(pipes[i][0]);
            break;
        } else {
            if(i > 0) {
                close(pipes[i-1][0]);
            }
            close(pipes[i][1]);
            id = i;
        }
    }
    if(id == -1) {
        for(int i=2; i <= END; ++i) {
            // printf("process %d write %d\n", id, i);
            write(pipes[0][1], &i, sizeof(int));
        }
        close(pipes[0][1]);
    } else {
        int prime;

        if(read(pipes[id][0], &prime, sizeof(int)) == 0) {
            goto finish;
        }
        printf("prime %d\n", prime);
        int num;
        while(read(pipes[id][0], &num, sizeof(int))) {
            if(num % prime != 0) {
                // printf("process %d write %d\n", id, num);
                assert(write(pipes[id+1][1], &num, sizeof(int)) >= 0);
            }
        }
        finish:;
        close(pipes[id][0]);
        if(id < NUM_PROCESS-1) {
            close(pipes[id+1][1]);
        }
    }
    if(id != NUM_PROCESS-1) {
        wait(0);
    }
    // printf("process %d exit\n", id);
    exit(0);
}