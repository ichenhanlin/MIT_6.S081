#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    if(argc != 2){
        fprintf(2, "Usage: sleep time\n");
        exit(1);
    }

    char *str_time = argv[1];
    while(*str_time != '\0'){
        if(*str_time < '0' || *str_time > '9'){
            fprintf(2, "sleep: %s is not a legal number\n", argv[1]);
            exit(1);
        }
        ++str_time;
    }
    int sleep_time = atoi(argv[1]);
    sleep(sleep_time);
    exit(0);
}
