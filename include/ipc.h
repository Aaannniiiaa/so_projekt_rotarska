#ifndef IPC_H
#define IPC_H
#define _POSIX_C_SOURCE 200809L
#include "common.h"

typedef struct {
    long mtype;
    int ok;
} msg_kasa_resp_t;

typedef struct {
    long mtype;
    int ok;
    int driver_id;
    int course_no;
    int bike;
    int vip;
} msg_invite_t;
int sem_setval(int semid, int idx, int val);
int sem_getval(int semid, int idx);
int sem_down(int semid, int idx);
int sem_up(int semid, int idx);
int sem_timeddown_s(int semid, int idx, int timeout_s);
int sem_timeddown_ms(int semid, int idx, int timeout_ms);
int sem_trydown(int semid, int idx);
int msg_send_kasa_resp(int msgid, pid_t pid, int ok);
int msg_recv_kasa_resp(int msgid, pid_t pid, int *ok_out);
int msg_send_invite(int msgid, pid_t pid, const msg_invite_t *inv);
int msg_recv_invite(int msgid, pid_t pid, msg_invite_t *inv_out);

#endif