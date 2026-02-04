#define _POSIX_C_SOURCE 200809L
#include "station_ipc.h"

#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef SEMUN_DEFINED
#define SEMUN_DEFINED
union semun { int val; struct semid_ds *buf; unsigned short *array; };
#endif

enum { SEM_MUTEX = 0, SEM_EVENT = 1, SEM_COUNT = 2 };

static key_t sem_key(void) {
    key_t k = ftok(".", 'S');
    if (k == -1) { perror("ftok sem"); exit(1); }
    return k;
}

static void sem_setval(int semid, int idx, int val) {
    union semun u;
    u.val = val;
    if (semctl(semid, idx, SETVAL, u) == -1) { perror("semctl SETVAL"); exit(1); }
}

int stsem_create(int *created) {
    key_t k = sem_key();
    int semid = semget(k, SEM_COUNT, 0600 | IPC_CREAT | IPC_EXCL);
    if (semid >= 0) {
        if (created) *created = 1;
        sem_setval(semid, SEM_MUTEX, 1);
        sem_setval(semid, SEM_EVENT, 0);
        return semid;
    }
    if (errno != EEXIST) { perror("semget"); exit(1); }
    semid = semget(k, SEM_COUNT, 0600 | IPC_CREAT);
    if (semid == -1) { perror("semget existing"); exit(1); }
    if (created) *created = 0;
    return semid;
}

void stsem_remove(int semid) {
    if (semid < 0) return;
    if (semctl(semid, 0, IPC_RMID) == -1) perror("semctl IPC_RMID");
}

static void sem_op(int semid, int idx, short op) {
    struct sembuf sb;
    sb.sem_num = (unsigned short)idx;
    sb.sem_op  = op;
    sb.sem_flg = 0;

    for (;;) {
        if (semop(semid, &sb, 1) == 0) return;
        if (errno == EINTR) continue;
        perror("semop");
        exit(1);
    }
}

void st_mutex_P(int semid) { sem_op(semid, SEM_MUTEX, -1); }
void st_mutex_V(int semid) { sem_op(semid, SEM_MUTEX, +1); }

void st_event_wait(int semid)   { sem_op(semid, SEM_EVENT, -1); }
void st_event_signal(int semid) { sem_op(semid, SEM_EVENT, +1); }