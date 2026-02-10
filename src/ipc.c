#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include <sys/ipc.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

key_t ipc_key(char proj){
    key_t k = ftok(".", proj);
    if(k == (key_t)-1){
        perror("ftok");
        exit(1);
    }
    return k;
}
