#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int p[2];
    int read_fd, write_fd;
    int v, n, i;
    int state = 1;
    int init = 0;
    pipe(p);
    read_fd = p[0];
    write_fd = p[1];
    if(fork() == 0){
        while(state){
            close(write_fd);
            state = read(read_fd, (void*)&v, sizeof(int));
            if(state == -1){
                fprintf(2, "primes: read error from pipe....\n");
                exit(1);
            }
            fprintf(0, "prime %d\n", v);
            for(;;){
                state = read(read_fd, (void*)&n, sizeof(int));
                if(state == -1){
                    fprintf(2, "primes: read error from pipe....\n");
                    exit(1);
                }else if(state == 0){
                    break;
                }

                if(n % v == 0){
                    continue;
                }else if(!init){
                    pipe(p);
                    if(fork() == 0){
                        //close(p[1]);
                        write_fd = p[1];
                        read_fd = p[0];
                        init = 0;
                        state = 1;
                        break;
                    }else{
                        close(p[0]);
                        write_fd = p[1];
                        init = 1;
                    }
                }
                write(write_fd, (void*)&n, sizeof(int));
            }
        }
        if(init){
            //fprintf(0, "close fd=%d\n", write_fd);
            close(write_fd);
        }
        //close(read_fd);
    }else{
        close(read_fd);
        for(i = 2; i <= 35; ++i){
            write(write_fd, (void*)&i, sizeof(i));
        }
        close(write_fd);
    }
    wait(0);
    exit(0);
}
