#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char *path, char *target){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, O_RDONLY)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    if(st.type != T_DIR){
        fprintf(2, "find: invalid path %s\n", path);
        return;
    }
    strcpy(buf, path);
    p = buf + strlen(path);
    *p = '/';
    ++p;
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0){
            continue;
        }
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
            fprintf(2, "find: path too long\n");
            continue;
        }
        if(strcmp(de.name, target) == 0){
            fprintf(1, "%s/%s\n", path, de.name);
        }
        strcpy(p, de.name);
        if(stat(buf, &st) < 0){
            fprintf(2, "find: cannot stat %s\n", path);
            continue;
        }
        if(st.type == T_DIR){
            find(buf, target);
        }
    }
}

int main(int argc, char *argv[]){
    if(argc < 3){
        printf("find: path ...\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
