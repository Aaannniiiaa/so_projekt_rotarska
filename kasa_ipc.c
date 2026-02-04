#define _POSIX_C_SOURCE 200809L
#include "kasa_ipc.h"
#include "common.h"

#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static key_t kasa_key(void) {
    key_t k = ftok(FTOK_PATH, FTOK_PROJ_KASA);
    if (k == (key_t)-1) { perror("ftok(KASA)"); exit(1); }
    return k;
}

static int kasa_create_only(void) {
    key_t k = kasa_key();
    int id = msgget(k, 0600 | IPC_CREAT | IPC_EXCL);
    if (id >= 0) return id;
    if (errno != EEXIST) { perror("msgget(KASA)"); exit(1); }

    id = msgget(k, 0600 | IPC_CREAT);
    if (id == -1) { perror("msgget(KASA open)"); exit(1); }
    return id;
}

int kasaq_create_clean(void) {
    key_t k = kasa_key();
    int old = msgget(k, 0600);
    if (old != -1) msgctl(old, IPC_RMID, NULL);
    return kasa_create_only();
}

void kasaq_remove(int msgid) {
    if (msgid < 0) return;
    if (msgctl(msgid, IPC_RMID, NULL) == -1) perror("msgctl(KASA IPC_RMID)");
}