#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "kasa_ipc.h"
#include "sem_ipc.h"
#include "common.h"
#include <sys/msg.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

static volatile sig_atomic_t g_term = 0;
static const char* who(int vip){
    return vip ? "VIP" : "NORMAL";
}

static void pas_print(int idx, const char* fmt, ...){
    if(!should_print(idx, PAS_PRINT_EVERY))
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

static int semop_intr(int semid, struct sembuf *ops, size_t nops){
    for(;;){
        if(semop(semid, ops, (unsigned)nops) == 0) 
            return 0;
        if(errno == EINTR){
            if(g_term) 
                return -1;
            continue;
        }
        return -1;
    }
}
static int wait_bus_wake(int semid){
    for(;;){
        struct sembuf op = { (unsigned short)SEM_BUS_WAKE, -1, 0 };
        if(semop(semid, &op, 1) == 0) 
            return 0;
        if(errno == EINTR){
            if(g_term) 
                return -1;
            continue;
        }
        return -1;
    }
}

static int is_global_stop(station_state* st){
    return (st->stop || (int)st->event == EV_KONIEC);
}

static void dec_waiting_counter(station_state* st, int semid, int vip){
    if(g_term) return;

    sem_lock(semid);
    if(vip){
        if(st->waiting_vip > 0)
            st->waiting_vip--;
    } else {
        if(st->waiting_norm > 0)
            st->waiting_norm--;
    }
    sem_unlock(semid);
}

static void h_term(int sig){
    (void)sig; g_term = 1;
}

static int acquire_entrance_block(int semid, int bike){
    int entr = bike ? SEM_ENTR2 : SEM_ENTR1;
    for(;;){
        struct sembuf op = { (unsigned short)entr, -1, 0 };
        if(semop(semid, &op, 1) == 0) 
            return entr;
        if(errno == EINTR){
            if(g_term) return -1;
            continue;
        }
        return -1;
    }
}

static void release_entrance(int semid, int entr){
    if(entr < 0) 
        return;
    struct sembuf op = { (unsigned short)entr, +1, 0 };
    (void)semop_intr(semid, &op, 1);
}

static int reserve_begin_atomic(int semid, int bike, int seats){
    if(seats < 1) 
        seats = 1;
    if(seats > 2) 
        seats = 2;

    if(!bike){
        struct sembuf ops[3] = {
            { (unsigned short)SEM_QANY, 0, 0 },
            { (unsigned short)SEM_BOARDING, +1, 0 },
            { (unsigned short)SEM_SEATS, (short)-seats, 0 }
        };
        return semop_intr(semid, ops, 3);
    } else {
        struct sembuf ops[4] = {
            { (unsigned short)SEM_QANY, 0, 0 },
            { (unsigned short)SEM_BOARDING, +1, 0 },
            { (unsigned short)SEM_BIKES,-1, 0 },
            { (unsigned short)SEM_SEATS, (short)-seats, 0 }
        };
        return semop_intr(semid, ops, 4);
    }
}

static void reserve_end(int semid){
    struct sembuf op = { (unsigned short)SEM_BOARDING, -1, 0 };
    (void)semop_intr(semid, &op, 1);
}
static void reserve_rollback(int semid, int bike, int seats){
    if(seats < 1) 
        seats = 1;
    if(seats > 2) 
        seats = 2;

    if(!bike){
        struct sembuf op = { (unsigned short)SEM_SEATS, (short)+seats, 0 };
        (void)semop_intr(semid, &op, 1);
    }else{
        struct sembuf ops[2] = {
            { (unsigned short)SEM_SEATS, (short)+seats, 0 },
            { (unsigned short)SEM_BIKES, +1, 0 }
        };
        (void)semop_intr(semid, ops, 2);
    }
}

int main(int argc, char** argv){
    if(argc < 5){
        fprintf(stderr, "usage: %s <idx> <shmid> <kasa_vip_qid> <kasa_norm_qid>\n", argv[0]);
        return 1;
    }

    struct sigaction sa = {0};
    sa.sa_handler = h_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    int idx = atoi(argv[1]);
    int shmid = atoi(argv[2]);
    int qvip = atoi(argv[3]);
    int qnorm = atoi(argv[4]);

    station_state* st = shm_attach(shmid);
    int semid = st->semid;

    if(is_global_stop(st)){
        shm_detach(st);
        return 0;
    }

    srand((unsigned)(getpid() ^ (idx*7919)));
    int vip = (rand()%2 == 0);
    int child = (rand()%5 == 0);
    int bike = (rand()%6 == 0);
    int seats = child ? 2 : 1;

    int kasa_qid = vip ? qvip : qnorm;

    kasa_req r;
    r.mtype = 1;
    r.pid = getpid();
    r.idx = idx;
    r.vip = vip;
    r.child = child;
    r.bike = bike;
    r.seats = seats;

    if(kasa_send_req(kasa_qid, &r) == -1){
        shm_detach(st);
        return 0;
    }

    sem_lock(semid);
    if(vip) 
        st->waiting_vip++;
    else
        st->waiting_norm++;
    sem_unlock(semid);

    {
        struct sembuf ops[2];
        ops[0].sem_num = (unsigned short)SEM_KASA_ANY;
        ops[0].sem_op = +1;
        ops[0].sem_flg = 0;

        ops[1].sem_num = (unsigned short)(vip ? SEM_QVIP : SEM_QNORM);
        ops[1].sem_op = +1;
        ops[1].sem_flg = 0;

        (void)sem_do(semid, ops, 2);
    }

    kasa_resp resp;
    for(;;){
        ssize_t rc = msgrcv(kasa_qid, &resp, sizeof(resp) - sizeof(long), (long)getpid(), 0);
        if(rc == -1){
            if(errno == EINTR){
                if(g_term || is_global_stop(st)){
                    dec_waiting_counter(st, semid, vip);
                    shm_detach(st);
                    return 0;
                }
                continue;
            }
            dec_waiting_counter(st, semid, vip);
            shm_detach(st);
            return 0;
        }
        break;
    }

    dec_waiting_counter(st, semid, vip);

    if(resp.ok == 0){
        if(!is_global_stop(st) && !g_term){
            pas_print(idx, "[PAS idx=%d pid=%d] %s child=%d bike=%d seats=%d -> ODMOWA (STOP)\n", idx, (int)getpid(), who(vip), child, bike, seats);
        }
        shm_detach(st);
        return 0;
    }

    int boarded = 0;
    int entr_used = -1;
    int reserved = 0;

    for(;;){
        if(g_term || is_global_stop(st)) 
            break;

        if(reserve_begin_atomic(semid, bike, seats) == -1){
            break;
        }
        reserved = 1;

        int entr = acquire_entrance_block(semid, bike);
        if(entr == -1){
            reserve_rollback(semid, bike, seats);
            reserve_end(semid);
            reserved = 0;
            if(wait_bus_wake(semid)==-1)
                break;
            continue;
        }

        sem_lock(semid);
        int ok_to_board = (st->event != EV_KONIEC && st->event != EV_ODJAZD && st->stop == 0);
        if(ok_to_board){
            st->onboard += seats;
            if(bike) st->onboard_bikes += 1;
        }
        sem_unlock(semid);

        if(!ok_to_board){
            reserve_rollback(semid, bike, seats);
            release_entrance(semid, entr);
            reserve_end(semid);
            reserved = 0;
            continue;
        }

        entr_used = entr;
        release_entrance(semid, entr);
        reserve_end(semid);
        reserved = 0;

        boarded = 1;
        break;
    }

    if(reserved){
        reserve_rollback(semid, bike, seats);
        reserve_end(semid);
        reserved = 0;
    }

    if(boarded) {
        pas_print(idx,
                  "[PAS idx=%d pid=%d] %s child=%d bike=%d seats=%d ticket=%d -> WSZEDL przez %s\n", idx, (int)getpid(), who(vip), child, 
                  bike, seats, resp.ticket, (entr_used == SEM_ENTR1 ? "ENTR1" : "ENTR2"));
    } else {
        if(!g_term && !is_global_stop(st)) {
            pas_print(idx, "[PAS idx=%d pid=%d] %s child=%d bike=%d seats=%d ticket=%d -> STOP/ODPADL\n", idx, (int)getpid(), who(vip), 
            child, bike, seats, resp.ticket);
        }
    }

    shm_detach(st);
    return 0;
}