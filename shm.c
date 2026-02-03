#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "common.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static key_t shm_key(void) {
    key_t key = ftok(FTOK_PATH, FTOK_PROJ_SHM);
    if (key == (key_t)-1) {
        perror("ftok(SHM)");
        exit(1);
    }
    return key;
}

int shm_create(int *created) {
    key_t key = shm_key();

    int shmid = shmget(key, sizeof(station_state), 0600 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        if (created) *created = 1;
        return shmid;
    }

    if (errno != EEXIST) {
        perror("shmget(IPC_CREAT|IPC_EXCL)");
        exit(1);
    }

    shmid = shmget(key, sizeof(station_state), 0600);
    if (shmid == -1) {
        perror("shmget(existing)");
        exit(1);
    }

    if (created) *created = 0;
    return shmid;
}

station_state* shm_attach(int shmid, int do_init) {
    void *addr = shmat(shmid, NULL, 0);
    if (addr == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    station_state *st = (station_state*)addr;

    if (do_init) {
        memset(st, 0, sizeof(*st));
        st->is_open = 0;
        st->shutdown = 0;
        st->ring.write_idx = 0;
        st->ring.read_idx  = 0;
        st->ring.count     = 0;
        // buf[] jest wyzerowane przez memset
    }

    return st;
}

void shm_detach(void *addr) {
    if (!addr) return;
    if (shmdt(addr) == -1) {
        perror("shmdt");
    }
}

void shm_remove(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl(IPC_RMID)");
    }
}