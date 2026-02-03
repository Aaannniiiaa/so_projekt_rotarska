#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "common.h"

#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>

static key_t get_key(void) {
    key_t key = ftok(FTOK_PATH, FTOK_PROJ);
    if (key == -1) {
        perror("ftok");
        exit(1);
    }
    return key;
}

int msg_create(void) {
    key_t key = get_key();
    int msgid = msgget(key, 0600 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }
    return msgid;
}

void msg_remove(int msgid) {
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl(IPC_RMID)");
        // nie exitujemy na siłę w cleanup
    }
}