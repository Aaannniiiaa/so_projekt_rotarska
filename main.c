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
    // bezpieczne kopiowanie
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

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        msg_remove(msgid);
        return 1;
    }

    if (pid == 0) {
        // dziecko: exec logger z msgid jako argument
        char idbuf[32];
        snprintf(idbuf, sizeof(idbuf), "%d", msgid);
        execl("./logger", "logger", idbuf, (char*)NULL);
        perror("exec logger");
        _exit(127);
    }

    // rodzic: wysyła kilka logów
    log_send(msgid, "START");
    log_send(msgid, "TEST: main -> logger (msg queue)");
    log_send(msgid, "STOP");

    // czekamy na logger
    int status = 0;
    if (waitpid(pid, &status, 0) == -1) perror("waitpid");

    // sprzątamy kolejkę
    msg_remove(msgid);

    // mały komunikat w konsoli
    printf("Done. raport.txt created.\n");
    return 0;
}