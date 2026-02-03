#define _POSIX_C_SOURCE 200809L
#include "synch.h"

#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void sem_op(int semid, unsigned short semnum, short delta) {
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op  = delta;
    op.sem_flg = 0; 

    for (;;) {
        if (semop(semid, &op, 1) == 0) return;
        if (errno == EINTR) continue;
        perror("semop");
        exit(1);
    }
}

void sem_P(int semid, unsigned short semnum) { sem_op(semid, semnum, -1); }
void sem_V(int semid, unsigned short semnum) { sem_op(semid, semnum, +1); }