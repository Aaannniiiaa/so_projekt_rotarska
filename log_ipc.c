#define _POSIX_C_SOURCE 200809L
#include "log_ipc.h"
#include "ipc.h"
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void die(const char* what){
    perror(what);
    exit(1);
}

int log_get(int create){
    key_t k = ipc_key('L');
    int flags = 0600 | (create ? IPC_CREAT : 0);
    int qid = msgget(k, flags);
    if(qid == -1) 
        die("msgget log");
    return qid;
}

int log_create_clean(int *created){
    *created = 0;
    key_t k = ipc_key('L');

    for(;;){
        int qid = msgget(k, 0600 | IPC_CREAT | IPC_EXCL);
        if(qid != -1){
            *created = 1;
            return qid;
        }

        if(errno == EEXIST){
            int old = msgget(k, 0600);
            if(old == -1) 
                die("msgget log existing");
            if(msgctl(old, IPC_RMID, NULL) == -1){
                die("msgctl log IPC_RMID (cannot clean old queue)");
            }
            continue;
        }

        die("msgget log create");
    }
}

void log_remove(int qid){
    if(qid < 0) 
        return;
    if(msgctl(qid, IPC_RMID, NULL) == -1){
        perror("msgctl log IPC_RMID");
    }
}

int log_send(int qid, const char* text){
    log_msg m;
    m.mtype = 1;
    strncpy(m.text, text, sizeof(m.text)-1);
    m.text[sizeof(m.text)-1] = '\0';

    if(msgsnd(qid, &m, sizeof(m.text), 0) == -1){
        return -1;
    }
    return 0;
}

int log_send_stop(int qid){
    log_msg m;
    m.mtype = 2;
    m.text[0] = '\0';
    if(msgsnd(qid, &m, sizeof(m.text), 0) == -1){
        return -1;
    }
    return 0;
}