#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "common.h"
#include "synch.h"
#include "shm.h"

#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static void log_send(int msgid, const char *txt) {
    log_msg m;
    m.mtype = LOG_MTYPE;
    snprintf(m.text, LOG_TEXT_MAX, "%s", txt);

    for (;;) {
        if (msgsnd(msgid, &m, log_msg_size(), 0) == 0) return;
        if (errno == EINTR) continue;
        perror("msgsnd log");
        exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <msgid> <shmid> <semid> <count>\n", argv[0]);
        return 2;
    }
    int msgid = atoi(argv[1]);
    int shmid = atoi(argv[2]);
    int semid = atoi(argv[3]);
    int count = atoi(argv[4]);

    station_state* st = shm_attach(shmid, 0);

    char line[LOG_TEXT_MAX];
    for (int i = 0; i < count; i++) {
        sem_P(semid, SEM_FULL);
        sem_P(semid, SEM_MUTEX);

        int idx = st->ring.read_idx;
        int val = st->ring.buf[idx];
        st->ring.read_idx = (idx + 1) % RING_SIZE;
        if (st->ring.count > 0) st->ring.count--;

        sem_V(semid, SEM_MUTEX);
        sem_V(semid, SEM_EMPTY);

        snprintf(line, sizeof(line), "[CONS pid=%d] got=%d", getpid(), val);
        log_send(msgid, line);
    }

    shm_detach(st);
    log_send(msgid, "[CONS] done");
    return 0;
}