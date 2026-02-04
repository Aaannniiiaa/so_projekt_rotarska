#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "common.h"

#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static key_t get_key(char proj) {
    key_t k = ftok(FTOK_PATH, proj);
    if (k == (key_t)-1) { perror("ftok"); exit(1); }
    return k;
}

static int msg_create_only(void) {
    key_t k = get_key(FTOK_PROJ_LOG);
    int id = msgget(k, 0600 | IPC_CREAT | IPC_EXCL);
    if (id >= 0) return id;
    if (errno != EEXIST) { perror("msgget(LOG)"); exit(1); }

    id = msgget(k, 0600 | IPC_CREAT);
    if (id == -1) { perror("msgget(LOG open)"); exit(1); }
    return id;
}

int msg_create_clean(void) {
    key_t k = get_key(FTOK_PROJ_LOG);
    int old = msgget(k, 0600);
    if (old != -1) msgctl(old, IPC_RMID, NULL);
    return msg_create_only();
}

void msg_remove(int msgid) {
    if (msgid < 0) return;
    if (msgctl(msgid, IPC_RMID, NULL) == -1) perror("msgctl(LOG IPC_RMID)");
}