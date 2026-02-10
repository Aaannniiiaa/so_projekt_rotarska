#ifndef KASA_IPC_H
#define KASA_IPC_H

#include <sys/types.h>

typedef struct {
    long mtype;
    pid_t pid;
    int idx;
    int vip;
    int child;
    int bike;
    int seats;
} kasa_req;

typedef struct {
    long mtype;
    int ok;
    int ticket;
} kasa_resp;

int kasa_vip_get(int create);
int kasa_norm_get(int create);
int kasa_vip_create_clean(int *created);
int kasa_norm_create_clean(int *created);
void kasa_remove(int qid);
int kasa_send_req (int qid, const kasa_req* r);
int kasa_send_resp(int qid, pid_t pid, int ok, int ticket);
int kasa_send_stop(int qid);
int kasa_send_stop_both(int qid_vip, int qid_norm);

#endif
