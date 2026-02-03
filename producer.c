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
    for (int i = 1; i <= count; i++) {
        int x = (int)getpid() * 100000 + i;

        sem_P(semid, SEM_EMPTY);
        sem_P(semid, SEM_MUTEX);

        int idx = st->ring.write_idx;
        st->ring.buf[idx] = x;
        st->ring.write_idx = (idx + 1) % RING_SIZE;
        if (st->ring.count < RING_SIZE) st->ring.count++;

        sem_V(semid, SEM_MUTEX);
        sem_V(semid, SEM_FULL);

        snprintf(line, sizeof(line), "[PROD pid=%d] put=%d", getpid(), x);
        log_send(msgid, line);
    }

    shm_detach(st);
    log_send(msgid, "[PROD] done");
    return 0;
}