#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "common.h"
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

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

int main(void) {
    int msgid = msg_create();

    int shmid = shm_create();
    shm_ring* shm = shm_attach(shmid);
    int semid = sem_create();

    shm->write_idx = 0;
    shm->read_idx  = 0;
    for (int i = 0; i < RING_SIZE; i++) shm->buffer[i] = 0;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        shm_detach(shm);
        sem_remove(semid);
        shm_remove(shmid);
        msg_remove(msgid);
        return 1;
    }

    if (pid == 0) {
        char idbuf[32];
        snprintf(idbuf, sizeof(idbuf), "%d", msgid);
        execl("./logger", "logger", idbuf, (char*)NULL);
        perror("exec logger");
        _exit(127);
    }

    log_send(msgid, "START");
    log_send(msgid, "ETAP2: SHM+SEM created and initialized");
    log_send(msgid, "STOP");

    int status = 0;
    if (waitpid(pid, &status, 0) == -1) perror("waitpid");

    shm_detach(shm);
    sem_remove(semid);
    shm_remove(shmid);
    msg_remove(msgid);

    printf("Done. raport.txt created.\n");
    return 0;
}