#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "common.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static key_t get_key(char proj) {
    key_t key = ftok(FTOK_PATH, proj);
    if (key == -1) {
        perror("ftok");
        exit(1);
    }
    return key;
}

int msg_create(void) {
    key_t key = get_key(FTOK_PROJ_LOG);
    int msgid = msgget(key, 0600 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }
    return msgid;
}

void msg_remove(int msgid) {
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl(IPC_RMID)");
    }
}

int shm_create(void) {
    key_t key = get_key(FTOK_PROJ_SHM);
    int shmid = shmget(key, sizeof(shm_ring), 0600 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    return shmid;
}

shm_ring* shm_attach(int shmid) {
    void *p = shmat(shmid, NULL, 0);
    if (p == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    return (shm_ring*)p;
}

void shm_detach(shm_ring* p) {
    if (!p) return;
    if (shmdt((void*)p) == -1) {
        perror("shmdt");
    }
}

void shm_remove(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl(IPC_RMID)");
    }
}

int sem_create(void) {
    key_t key = get_key(FTOK_PROJ_SEM);
    int semid = semget(key, SEM_COUNT, 0600 | IPC_CREAT);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    unsigned short init[SEM_COUNT];
    init[SEM_MUTEX] = 1;
    init[SEM_EMPTY] = RING_SIZE;
    init[SEM_FULL]  = 0;

    union semun arg;
    arg.array = init;

    if (semctl(semid, 0, SETALL, arg) == -1) {
        perror("semctl SETALL");
        exit(1);
    }

    return semid;
}

void sem_remove(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl(IPC_RMID)");
    }
}