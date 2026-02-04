#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "common.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static key_t shm_key(void) {
    key_t k = ftok(FTOK_PATH, FTOK_PROJ_STSHM);
    if (k == (key_t)-1) { perror("ftok(SHM)"); exit(1); }
    return k;
}

int shm_create(int *created) {
    if (created) *created = 0;

    key_t k = shm_key();

    // próbuj stworzyć "na świeżo"
    int shmid = shmget(k, (int)sizeof(station_state), 0600 | IPC_CREAT | IPC_EXCL);
    if (shmid >= 0) {
        if (created) *created = 1;
        return shmid;
    }

    // jak już istnieje -> otwórz istniejący
    if (errno != EEXIST) { perror("shmget create"); exit(1); }

    shmid = shmget(k, (int)sizeof(station_state), 0600 | IPC_CREAT);
    if (shmid == -1) { perror("shmget open"); exit(1); }
    return shmid;
}

station_state* shm_attach(int shmid) {
    void *p = shmat(shmid, NULL, 0);
    if (p == (void*)-1) { perror("shmat"); exit(1); }
    return (station_state*)p;
}

void shm_detach(station_state *st) {
    if (!st) return;
    if (shmdt(st) == -1) perror("shmdt");
}

void shm_remove(int shmid) {
    if (shmid < 0) return;
    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("shmctl(IPC_RMID)");
}