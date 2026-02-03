#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "common.h"
#include "shm.h"

#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define PROD_COUNT 5
#define CONS_COUNT 5
#define ITEMS_PER_PROD 200

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

static pid_t spawn1(const char *prog, const char *a1) {
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }
    if (pid == 0) {
        execl(prog, prog, a1, (char*)NULL);
        perror("exec");
        _exit(127);
    }
    return pid;
}

static pid_t spawn4(const char *prog, const char *a1, const char *a2, const char *a3, const char *a4) {
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }
    if (pid == 0) {
        execl(prog, prog, a1, a2, a3, a4, (char*)NULL);
        perror("exec");
        _exit(127);
    }
    return pid;
}

static void wait_all(const char *name, pid_t *pids, int n) {
    int st = 0;
    for (int i = 0; i < n; i++) {
        if (waitpid(pids[i], &st, 0) == -1) { perror("waitpid"); continue; }
        if (WIFEXITED(st) && WEXITSTATUS(st) != 0) {
            fprintf(stderr, "%s pid=%d exited with %d\n", name, (int)pids[i], WEXITSTATUS(st));
        } else if (WIFSIGNALED(st)) {
            fprintf(stderr, "%s pid=%d killed by signal %d\n", name, (int)pids[i], WTERMSIG(st));
        }
    }
}

int main(void) {
    int msgid = msg_create();
    int semid = sem_create();

    int created = 0;
    int shmid = shm_create(&created);
    station_state* st = shm_attach(shmid, 1); // init zawsze tu, bo to test w main

    // reset stanu
    st->is_open = 1;     // w tym teście od razu "otwarte"
    st->shutdown = 0;
    st->ring.write_idx = 0;
    st->ring.read_idx = 0;
    st->ring.count = 0;
    for (int i = 0; i < RING_SIZE; i++) st->ring.buf[i] = 0;

    char msgbuf[32], shmbuf[32], sembuf[32];
    snprintf(msgbuf, sizeof(msgbuf), "%d", msgid);
    snprintf(shmbuf, sizeof(shmbuf), "%d", shmid);
    snprintf(sembuf, sizeof(sembuf), "%d", semid);

    pid_t logger_pid = spawn1("./logger", msgbuf);

    log_send(msgid, "START");
    {
        char line[LOG_TEXT_MAX];
        snprintf(line, sizeof(line),
                 "STRESS: PROD=%d CONS=%d ITEMS_PER_PROD=%d TOTAL=%d",
                 PROD_COUNT, CONS_COUNT, ITEMS_PER_PROD, PROD_COUNT * ITEMS_PER_PROD);
        log_send(msgid, line);
    }

    pid_t prods[PROD_COUNT];
    pid_t cons[CONS_COUNT];

    int total = PROD_COUNT * ITEMS_PER_PROD;
    int per_cons = total / CONS_COUNT;
    int rest = total % CONS_COUNT;

    for (int i = 0; i < PROD_COUNT; i++) {
        char countbuf[32];
        snprintf(countbuf, sizeof(countbuf), "%d", ITEMS_PER_PROD);
        prods[i] = spawn4("./producer", msgbuf, shmbuf, sembuf, countbuf);
    }

    for (int i = 0; i < CONS_COUNT; i++) {
        char countbuf[32];
        int take = per_cons + (i == CONS_COUNT - 1 ? rest : 0);
        snprintf(countbuf, sizeof(countbuf), "%d", take);
        cons[i] = spawn4("./consumer", msgbuf, shmbuf, sembuf, countbuf);
    }

    wait_all("PROD", prods, PROD_COUNT);
    wait_all("CONS", cons, CONS_COUNT);

    log_send(msgid, "STOP");

    int stc = 0;
    if (waitpid(logger_pid, &stc, 0) == -1) perror("waitpid logger");

    shm_detach(st);
    sem_remove(semid);
    shm_remove(shmid);
    msg_remove(msgid);

    printf("OK. See raport.txt\n");
    return 0;
}
