#define _POSIX_C_SOURCE 200809L
#include "shm.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int shm_create(int *created) {
    key_t key = ftok(".", 'M');
    if (key == -1) { perror("ftok"); exit(1); }

    if (created) *created = 0;

    int shmid = shmget(key, (int)sizeof(station_state), 0600 | IPC_CREAT | IPC_EXCL);
    if (shmid == -1) {
        if (errno != EEXIST) { perror("shmget"); exit(1); }
        shmid = shmget(key, (int)sizeof(station_state), 0600 | IPC_CREAT);
        if (shmid == -1) { perror("shmget existing"); exit(1); }
    } else {
        if (created) *created = 1;
    }

    station_state *st = (station_state*)shmat(shmid, NULL, 0);
    if (st == (void*)-1) { perror("shmat"); exit(1); }

    if (created && *created) {
        st->event = ST_EV_NONE;
        st->koniec = 0;
        st->kurs = 0;
        st->na_przystanku = 0;
        st->w_autobusie = 0;
    }

    if (shmdt(st) == -1) { perror("shmdt"); exit(1); }
    return shmid;
}

station_state* shm_attach(int shmid) {
    void *p = shmat(shmid, NULL, 0);
    if (p == (void*)-1) { perror("shmat"); exit(1); }
    return (station_state*)p;
}

void shm_detach(station_state *st) {
    if (!st) return;
    if (shmdt(st) == -1) { perror("shmdt"); exit(1); }
}

void shm_remove(int shmid) {
    if (shmid < 0) return;
    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("shmctl IPC_RMID");
}