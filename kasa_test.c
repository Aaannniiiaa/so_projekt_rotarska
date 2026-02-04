#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "kasa_ipc.h"
#include "common.h"

#include <sys/wait.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void log_send(int logid, const char *txt) {
    log_msg m;
    m.mtype = LOG_MTYPE;
    snprintf(m.text, LOG_TEXT_MAX, "%s", txt);
    for (;;) {
        if (msgsnd(logid, &m, log_msg_size(), 0) == 0) return;
        if (errno == EINTR) continue;
        perror("msgsnd(LOG)");
        _exit(1);
    }
}

static pid_t spawnv(const char *prog, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }
    if (pid == 0) {
        execv(prog, argv);
        perror("execv");
        _exit(127);
    }
    return pid;
}

static void wait_one(const char *name, pid_t pid) {
    int st = 0;
    while (waitpid(pid, &st, 0) == -1 && errno == EINTR) {}
    if (WIFEXITED(st) && WEXITSTATUS(st) != 0) {
        fprintf(stderr, "%s pid=%d exit=%d\n", name, (int)pid, WEXITSTATUS(st));
    } else if (WIFSIGNALED(st)) {
        fprintf(stderr, "%s pid=%d sig=%d\n", name, (int)pid, WTERMSIG(st));
    }
}

int main(void) {
    const int N = 30;

    int logid  = msg_create_clean();
    int kasaid = kasaq_create_clean();

    char logbuf[32], kasabuf[32], nbuf[32];
    snprintf(logbuf, sizeof(logbuf), "%d", logid);
    snprintf(kasabuf, sizeof(kasabuf), "%d", kasaid);
    snprintf(nbuf, sizeof(nbuf), "%d", N);

    char *argv_logger[] = { "./logger", logbuf, NULL };
    pid_t pid_logger = spawnv("./logger", argv_logger);

    log_send(logid, "START KASA_TEST");

    char *argv_kasa[] = { "./kasa", kasabuf, logbuf, nbuf, NULL };
    pid_t pid_kasa = spawnv("./kasa", argv_kasa);

    pid_t *pass = calloc((size_t)N, sizeof(pid_t));
    if (!pass) { perror("calloc"); exit(1); }

    for (int i = 0; i < N; i++) {
        if (i % 10 == 0) {
            char *argv_p[] = { "./passenger", kasabuf, logbuf, "--vip", NULL };
            pass[i] = spawnv("./passenger", argv_p);
        } else {
            char *argv_p[] = { "./passenger", kasabuf, logbuf, "--norm", NULL };
            pass[i] = spawnv("./passenger", argv_p);
        }
    }

    for (int i = 0; i < N; i++) wait_one("PASS", pass[i]);
    free(pass);

    wait_one("KASA", pid_kasa);

    log_send(logid, "STOP");
    wait_one("LOGGER", pid_logger);

    kasaq_remove(kasaid);
    msg_remove(logid);

    printf("OK. See raport.txt\n");
    return 0;
}