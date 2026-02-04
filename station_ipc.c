#define _POSIX_C_SOURCE 200809L
#include "station_ipc.h"
#include "common.h"

#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static key_t sem_key(void) {
    key_t key = ftok(FTOK_PATH, FTOK_PROJ_STSEM);
    if (key == (key_t)-1) { perror("ftok(SEM)"); exit(1); }
    return key;
}

static void semop_one(int semid, int idx, int op, int flags) {
    struct sembuf sb;
    sb.sem_num = (unsigned short)idx;
    sb.sem_op  = (short)op;
    sb.sem_flg = (short)flags;

    for (;;) {
        if (semop(semid, &sb, 1) == 0) return;
        if (errno == EINTR) continue;
        perror("semop");
        exit(1);
    }
}

int stsem_create(int *created) {
    if (created) *created = 0;

    key_t key = sem_key();
    int semid = semget(key, SEM_COUNT, 0600 | IPC_CREAT | IPC_EXCL);
    if (semid >= 0) {
        if (created) *created = 1;
        return semid;
    }
    if (errno != EEXIST) { perror("semget create"); exit(1); }

    semid = semget(key, SEM_COUNT, 0600 | IPC_CREAT);
    if (semid == -1) { perror("semget open"); exit(1); }
    return semid;
}

void stsem_remove(int semid) {
    if (semid < 0) return;
    if (semctl(semid, 0, IPC_RMID) == -1) perror("semctl(IPC_RMID)");
}

void sem_P(int semid, int idx) { semop_one(semid, idx, -1, 0); }
void sem_V(int semid, int idx) { semop_one(semid, idx, +1, 0); }

int sem_getval(int semid, int idx) {
    int v = semctl(semid, idx, GETVAL);
    if (v == -1) { perror("semctl(GETVAL)"); exit(1); }
    return v;
}

void sem_setval(int semid, int idx, int val) {
    union semun u;
    u.val = val;
    if (semctl(semid, idx, SETVAL, u) == -1) {
        perror("semctl(SETVAL)");
        exit(1);
    }
}