#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "common.h"

#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static int ring_push(shm_ring *r, int v) {
    if (r->count >= RING_SIZE) return 0;
    r->buf[r->write_idx] = v;
    r->write_idx = (r->write_idx + 1) % RING_SIZE;
    r->count++;
    return 1;
}

static int ring_pop(shm_ring *r, int *out) {
    if (r->count <= 0) return 0;
    *out = r->buf[r->read_idx];
    r->read_idx = (r->read_idx + 1) % RING_SIZE;
    r->count--;
    return 1;
}

int main(void) {
    int created = 0;
    int shmid = shm_create(&created);
    station_state *st = shm_attach(shmid, created);

    printf("[SHM_DEMO] shmid=%d created=%d\n", shmid, created);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        shm_detach(st);
        shm_remove(shmid);
        return 1;
    }

    if (pid == 0) {
        for (int i = 1; i <= RING_SIZE; i++) {
            if (!ring_push(&st->ring, i)) {
                fprintf(stderr, "[CHILD] ring full at i=%d\n", i);
                _exit(2);
            }
        }
        printf("[CHILD] wrote %d items\n", RING_SIZE);
        shm_detach(st);
        _exit(0);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) == -1) perror("waitpid");

    int ok = 1;
    for (int i = 1; i <= RING_SIZE; i++) {
        int v = -1;
        if (!ring_pop(&st->ring, &v)) {
            fprintf(stderr, "[PARENT] ring empty at i=%d\n", i);
            ok = 0;
            break;
        }
        printf("[PARENT] got=%d\n", v);
        if (v != i) ok = 0;
    }

    printf("[SHM_DEMO] RESULT: %s\n", ok ? "OK" : "FAIL");

    shm_detach(st);
    shm_remove(shmid);

    return ok ? 0 : 1;
}