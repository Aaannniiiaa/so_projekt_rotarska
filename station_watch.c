#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "common.h"
#include <stdio.h>

int main(void) {
    int created = 0;
    int shmid = shm_create(&created);
    station_state *st = shm_attach(shmid, 0);

    printf("[WATCH] shmid=%d created_now=%d\n", shmid, created);
    printf("[WATCH] is_open=%d shutdown=%d ring(count=%d w=%d r=%d)\n",
           st->is_open, st->shutdown,
           st->ring.count, st->ring.write_idx, st->ring.read_idx);

    shm_detach(st);
    return 0;
}