#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"

int main(int argc, char *argv[])
{
    char* nargv[MAXARG];
    for(int i=1; i < argc; ++i) {
        nargv[i-1] = argv[i];
    }
    int nargc = argc-1;
    
    char lines[MAXARG][MAXARG];
    for(int i=0; nargc < MAXARG; ++i) {
        for(int j=0; j < MAXARG; ++j) {
            if(read(0, &lines[i][j], 1) == 0) {
                lines[i][j] = '\0';
                break;
            } else if(lines[i][j] == '\n') {
                lines[i][j] = '\0';
                break;
            }
        }
        nargv[nargc++] = lines[i];
        if(lines[i][0] == '\0') {
            break;
        }
    }

    if(fork()) {
        wait(0);
    } else {
        exec(nargv[0], nargv);
    }
    exit(0);
}