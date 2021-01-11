#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int p[2];
    int cur_pid;
    int pid;
    char byte;
    pipe(p);
    pid = fork();
    cur_pid = getpid();
    if(pid == 0){
        if(read(p[0], &byte, 1) <= 0){
            fprintf(2, "pingpong: the pipe closed early\n");
            exit(1);
        }
        fprintf(1, "%d: received ping\n", cur_pid);
        write(p[1], &byte, 1);
    }else{
        write(p[1], "0", 1);
        wait(0);
        if(read(p[0], &byte, 1) <= 0){
            fprintf(2, "pingpong: the pipe closed early\n");
            exit(1);
        }
        fprintf(1, "%d: received pong\n", cur_pid);
    }
    exit(0);
}
