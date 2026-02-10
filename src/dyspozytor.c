#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "kasa_ipc.h"
#include "log_ipc.h"
#include "sem_ipc.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

static station_state* g_st = NULL;
static int g_qvip = -1;
static int g_qnorm = -1;
static int g_log_qid  = -1;
static int g_semid    = -1;

static volatile sig_atomic_t f_usr1 = 0;
static volatile sig_atomic_t f_usr2 = 0;

static void h_usr1(int sig){
    (void)sig; f_usr1 = 1;
}
static void h_usr2(int sig){
    (void)sig; f_usr2 = 1;
}

static void wake_kasa_for_stop(int semid){
    struct sembuf ops[3];
    ops[0].sem_num = (unsigned short)SEM_KASA_ANY;
    ops[0].sem_op = +1;
    ops[0].sem_flg = 0;
    ops[1].sem_num = (unsigned short)SEM_QVIP;
    ops[1].sem_op = +1;
    ops[1].sem_flg = 0;
    ops[2].sem_num = (unsigned short)SEM_QNORM;
    ops[2].sem_op = +1;
    ops[2].sem_flg = 0;
    (void)sem_do(semid, ops, 3);
}

int main(int argc, char** argv){
    if(argc < 5){
        fprintf(stderr, "usage: %s <shmid> <kasa_vip_qid> <kasa_norm_qid> <log_qid>\n", argv[0]);
        return 1;
    }

    int shmid = atoi(argv[1]);
    g_qvip    = atoi(argv[2]);
    g_qnorm   = atoi(argv[3]);
    g_log_qid = atoi(argv[4]);

    g_st = shm_attach(shmid);
    g_semid = g_st->semid;

    sem_lock(g_semid);
    g_st->pid_dysp = getpid();
    sem_unlock(g_semid);

    struct sigaction sa1 = {0}, sa2 = {0};
    sa1.sa_handler = h_usr1;
    sa2.sa_handler = h_usr2;
    sigemptyset(&sa1.sa_mask);
    sigemptyset(&sa2.sa_mask);
    sa1.sa_flags = SA_RESTART;
    sa2.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa1, NULL);
    sigaction(SIGUSR2, &sa2, NULL);

    printf("[DYSP pid=%d] start\n", (int)getpid());

    sigset_t block, oldmask, waitmask;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    sigaddset(&block, SIGUSR2);

    if(sigprocmask(SIG_BLOCK, &block, &oldmask) == -1){
        perror("sigprocmask");
        shm_detach(g_st);
        return 1;
    }

    waitmask = oldmask;
    sigdelset(&waitmask, SIGUSR1);
    sigdelset(&waitmask, SIGUSR2);
    sigdelset(&waitmask, SIGINT);
    sigdelset(&waitmask, SIGTERM);

    while(1){
        while(!f_usr1 && !f_usr2){
            sigsuspend(&waitmask);
        }

        if(f_usr2){
            f_usr2 = 0;
            pid_t pk;
            sem_lock(g_semid);
            g_st->stop  = 1;
            g_st->event = EV_KONIEC;
            pk = g_st->pid_kier;
            sem_unlock(g_semid);

            printf("[DYSP] SIGUSR2 -> KONIEC\n");

            (void)kasa_send_stop_both(g_qvip, g_qnorm);
            wake_kasa_for_stop(g_semid);

            if(g_log_qid != -1) 
                (void)log_send_stop(g_log_qid);

            if(pk > 0) {
                kill(pk, SIGUSR2);
            }
            break;
        }

        if(f_usr1){
            f_usr1 = 0;
            pid_t pk;
            int ev, bus_here, force;

            sem_lock(g_semid);
            ev = (int)g_st->event;
            bus_here = g_st->bus_at_station;
            pk = g_st->pid_kier;
            force = g_st->force_depart;

            if(ev != EV_KONIEC && bus_here){
                if(force == 0){
                    g_st->force_depart = 1;
                }
            } else {
#if SLEEPY
                printf("[DYSP] SIGUSR1 -> ignoruję (brak autobusu / koniec)\n");
#endif
            }
            sem_unlock(g_semid);

            if(ev == EV_KONIEC) 
                break;

            if(pk > 0 && ev != EV_KONIEC && bus_here && force == 0){
                kill(pk, SIGUSR1);
            }
        }

        sem_lock(g_semid);
        int ev_end = (int)g_st->event;
        sem_unlock(g_semid);
        if(ev_end == EV_KONIEC) 
            break;
    }

    (void)sigprocmask(SIG_SETMASK, &oldmask, NULL);

    shm_detach(g_st);
    printf("[DYSP] stop\n");
    return 0;
}