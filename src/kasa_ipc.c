#define _POSIX_C_SOURCE 200809L
#include "kasa_ipc.h"
#include "ipc.h"
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void die(const char* what){
    perror(what);
    exit(1);
}

static int q_get(char proj, int create){
    key_t k = ipc_key(proj);
    int flags = 0600 | (create ? IPC_CREAT : 0);
    int qid = msgget(k, flags);
    if(qid == -1) 
        die("msgget kasa");
    return qid;
}

static int q_create_clean(char proj, int *created){
    *created = 0;
    key_t k = ipc_key(proj);

    for(;;){
        int qid = msgget(k, 0600 | IPC_CREAT | IPC_EXCL);
        if(qid != -1){
            *created = 1;
            return qid;
        }

        if(errno == EEXIST){
            qid = msgget(k, 0600);
            if(qid == -1) 
                die("msgget kasa existing");

            if(msgctl(qid, IPC_RMID, NULL) == -1){
                die("msgctl kasa IPC_RMID (cannot clean old queue)");
            }
            continue;
        }

        die("msgget kasa create");
    }
}

int kasa_vip_get(int create)  {
    return q_get('V', create);
}
int kasa_norm_get(int create) {
    return q_get('N', create);
}

int kasa_vip_create_clean(int *created)  {
    return q_create_clean('V', created);
}
int kasa_norm_create_clean(int *created) {
    return q_create_clean('N', created);
}

void kasa_remove(int qid){
    if(qid < 0) return;
    if(msgctl(qid, IPC_RMID, NULL) == -1){
        perror("msgctl kasa IPC_RMID");
    }
}

int kasa_send_req(int qid, const kasa_req* r){
    if(msgsnd(qid, r, sizeof(*r) - sizeof(long), 0) == -1){
        return -1;
    }
    return 0;
}

int kasa_send_resp(int qid, pid_t pid, int ok, int ticket){
    kasa_resp m;
    m.mtype = (long)pid;
    m.ok = ok;
    m.ticket = ticket;

    if(msgsnd(qid, &m, sizeof(m) - sizeof(long), 0) == -1){
        return -1;
    }
    return 0;
}

int kasa_send_stop(int qid){
    kasa_req r;
    r.mtype = 1;
    r.pid = 0;
    r.idx = -1;
    r.vip = r.child = r.bike = 0;
    r.seats = 1;

    if(msgsnd(qid, &r, sizeof(r) - sizeof(long), 0) == -1){
        return -1;
    }
    return 0;
}

int kasa_send_stop_both(int qid_vip, int qid_norm){
    int a = 0, b = 0;
    if(qid_vip  != -1) 
        a = kasa_send_stop(qid_vip);
    if(qid_norm != -1) 
        b = kasa_send_stop(qid_norm);
    return (a == -1 || b == -1) ? -1 : 0;
}