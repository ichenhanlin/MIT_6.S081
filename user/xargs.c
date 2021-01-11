#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

char buf[1024];

void fork_exec(char *argv[]){
    if(fork() == 0){
        exec(argv[0], argv);
    }else{
        wait(0);
    }
}

int main(int argc, char *argv[]){
    char *new_argv[MAXARG+1];
    char *p, *q;
    int m, n;
    int i=1;
    if(argc < 2){
        fprintf(2, "xargs: [commands]]\n");
        exit(1);
    }
    if(argc > MAXARG){
        fprintf(2, "xargs: out of max exec size %d", MAXARG);
        exit(1);
    }
    for(i=1; i<argc; ++i){
        new_argv[i-1] = argv[i];
    }
    m = 0;
    while((n = read(0, buf+m, sizeof(buf)-m-1)) > 0){
        m += n;
        buf[n] = '\0';
        p = buf;
        while((q=strchr(p, '\n')) != 0){
            *q = '\0';
            new_argv[argc-1] = p;
            new_argv[argc] = 0;
            fork_exec(new_argv);
            p = q+1;
        }
        if(m > 0){
            m -= p - buf;
            memmove(buf, p, m);
        }
    }
    exit(0);
}
