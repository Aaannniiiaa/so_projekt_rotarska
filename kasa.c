#define _POSIX_C_SOURCE 200809L
#include "kasa_ipc.h"
#include "log_ipc.h"
#include "shm.h"
#include "sem_ipc.h"
#include "common.h"
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <errno.h>

static int sem_try_take(int semid, int idx){
    struct sembuf op = { (unsigned short)idx, -1, IPC_NOWAIT };
    if(semop(semid, &op, 1) == 0) 
        return 0;
    if(errno == EAGAIN) 
        return 1;
    return -1;
}
static int sem_take(int semid, int idx){
    struct sembuf op = { (unsigned short)idx, -1, 0 };
    return sem_do(semid, &op, 1);
}

static int pick_vip_first(station_state* st, int semid){
    int wv, wn;
    sem_lock(semid);
    wv = st->waiting_vip;
    wn = st->waiting_norm;
    sem_unlock(semid);

    if(wv > 0) 
        return 1;
    if(wn > 0) 
        return 0;
    return 1;
}

int main(int argc, char** argv){
    if(argc < 5){
        fprintf(stderr, "usage: %s <shmid> <vip_qid> <norm_qid> <log_qid>\n", argv[0]);
        return 1;
    }

    int shmid = atoi(argv[1]);
    int vip_qid = atoi(argv[2]);
    int norm_qid = atoi(argv[3]);
    int log_qid = atoi(argv[4]);

    station_state* st = shm_attach(shmid);
    int semid = st->semid;

    sem_lock(semid);
    st->pid_kasa = getpid();
    sem_unlock(semid);

    printf("[KASA pid=%d] start (VIP> NORMAL)\n", (int)getpid());

    long served = 0;

    for(;;){
    if(sem_take(semid, SEM_KASA_ANY) == -1) 
        break;

    sem_lock(semid);
    int ev = (int)st->event;
    sem_unlock(semid);
    if(ev == EV_KONIEC) 
        break;

    int from_vipq = -1;
    int tr = sem_try_take(semid, SEM_QVIP);
    if(tr == 0){
        from_vipq = 1;
    } else if(tr == 1) {
        tr = sem_try_take(semid, SEM_QNORM);
        if(tr == 0) {
            from_vipq = 0;
        }
        else if(tr == 1){
            continue;
        } else {
            break;
        }
    } else {
        break;
    }

    int qid = from_vipq ? vip_qid : norm_qid;

    kasa_req req;
    for(;;){
        ssize_t rc = msgrcv(qid, &req, sizeof(req) - sizeof(long), 1, 0);
        if(rc >= 0) 
            break;
        if(errno == EINTR) 
            continue;
        if(errno == EIDRM) 
            goto out;
        goto out;
    }

        if(req.idx == -1){
            break;
        }

        sem_lock(semid);
        int ev2 = (int)st->event;
        int kurs = (int)st->kurs;
        sem_unlock(semid);

        if(ev2 == EV_KONIEC){
            (void)kasa_send_resp(qid, req.pid, 0, -1);
            continue;
        }

        int ticket;
        sem_lock(semid);
        ticket = st->next_ticket++;
        st->last_ticket = ticket;

        st->cnt_req_total += 1;
        if(from_vipq)
            st->cnt_vip += 1;
        if(req.child)
            st->cnt_child += 1;
        if(req.bike)
            st->cnt_bike  += 1;
        sem_unlock(semid);

        if(kasa_send_resp(qid, req.pid, 1, ticket) == -1){
            break;
        }

        served++;

        if(log_qid != -1 && should_print(served, KASA_PRINT_EVERY)){
            char line[220];
            snprintf(line, sizeof(line), "KASA kurs=%d %s pid=%d idx=%d ticket=%d child=%d bike=%d seats=%d", kurs,
            (from_vipq ? "VIP" : "NORMAL"), (int)req.pid, req.idx, ticket, req.child, req.bike, req.seats);
            (void)log_send(log_qid, line);
        }
    }

out:
    shm_detach(st);
    printf("[KASA] stop\n");
    return 0;
}