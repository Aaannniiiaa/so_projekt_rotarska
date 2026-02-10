#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "sem_ipc.h"
#include "kasa_ipc.h"
#include "log_ipc.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

static station_state* g_st = NULL;
static int g_semid = -1;

static volatile sig_atomic_t f_usr1 = 0;
static volatile sig_atomic_t f_usr2 = 0;

static int in_trip = 0;
static int stop_req = 0;

#if SLEEPY
static volatile sig_atomic_t f_alrm = 0;
static void h_alrm(int sig) {
    (void)sig; f_alrm = 1;
}
#endif

static void h_usr1(int sig) {
    (void)sig; f_usr1 = 1;
}
static void h_usr2(int sig) {
    (void)sig; f_usr2 = 1;
}

static void close_entrances(int semid){
    sem_setval(semid, SEM_ENTR1, 0);
    sem_setval(semid, SEM_ENTR2, 0);
}
static void open_entrances(int semid){
    sem_setval(semid, SEM_ENTR1, 1);
    sem_setval(semid, SEM_ENTR2, 1);
}
static void wake_passengers(int semid){
    struct sembuf op = { (unsigned short)SEM_BUS_WAKE, +30000, 0 };
    (void)sem_do(semid, &op, 1);
}

static void gate_close(int semid){
    (void)sem_setval(semid, SEM_QANY, 1);
}
static void gate_open (int semid){
    (void)sem_setval(semid, SEM_QANY, 0);
}

static void wait_no_reservations(int semid){
    (void)sem_wait_zero(semid, SEM_BOARDING);
}

static void station_bus_ready(void){
    int P, R;
    sem_lock(g_semid);
    P = g_st->P;
    R = g_st->R;
    g_st->onboard = 0;
    g_st->onboard_bikes = 0;
    g_st->event = EV_NONE;
    g_st->bus_at_station = 1;
    sem_unlock(g_semid);

    sem_setval(g_semid, SEM_SEATS, P);
    sem_setval(g_semid, SEM_BIKES, R);

    gate_open(g_semid);
    open_entrances(g_semid);
    wake_passengers(g_semid);

}

static void station_empty(void){
    sem_lock(g_semid);
    g_st->bus_at_station = 0;
    sem_unlock(g_semid);

    close_entrances(g_semid);
    gate_close(g_semid);

    sem_setval(g_semid, SEM_SEATS, 0);
    sem_setval(g_semid, SEM_BIKES, 0);
}

static int rand_in_range(int a, int b){
    if(a > b){ int t=a; a=b; b=t; }
    if(a < 1) a = 1;
    if(b < 1) b = 1;
    int span = b - a + 1;
    return a + (rand() % span);
}

#if SLEEPY
static void demo_return_after_trip(void){
    sem_lock(g_semid);
    if(g_st->buses_total > 0) 
        g_st->buses_free += 1;
    int free_after = g_st->buses_free;
    int bus_here = g_st->bus_at_station;
    int kurs_now = (int)g_st->kurs;
    sem_unlock(g_semid);

    in_trip = 0;

    if(!bus_here && free_after > 0){
        sem_lock(g_semid);
        g_st->buses_free -= 1;
        sem_unlock(g_semid);
        station_bus_ready();
    }

    sem_lock(g_semid);
    if(g_st->bus_at_station && g_st->event == EV_ODJAZD) 
        g_st->event = EV_NONE;
    sem_unlock(g_semid);

    if(should_print(kurs_now, KIER_PRINT_EVERY)){
        printf("[KIER] POWROT free_pool=%d\n", free_after);
    }

    if(stop_req){
        sem_lock(g_semid);
        g_st->stop  = 1;
        g_st->event = EV_KONIEC;
        sem_unlock(g_semid);

        (void)kasa_send_stop_both(g_st->kasa_qid_vip, g_st->kasa_qid_norm);
        if(g_st->log_qid != -1) 
            (void)log_send_stop(g_st->log_qid);

        printf("[KIER] STOP (po powrocie)\n");
    }
}
#endif

static void do_depart(void){
    gate_close(g_semid);

    wait_no_reservations(g_semid);

    close_entrances(g_semid);

    int kurs, onboard, bikes, free_before;

    sem_lock(g_semid);
    if(g_st->event == EV_KONIEC){
        sem_unlock(g_semid);
        return;
    }
    g_st->event = EV_ODJAZD;
    g_st->kurs += 1;
    kurs = (int)g_st->kurs;

    onboard = g_st->onboard;
    bikes   = g_st->onboard_bikes;

    int Pcap = g_st->P;
    if(onboard > Pcap){
        printf("\n!!! ERROR: onboard=%d > P=%d (NIE POWINNO SIE ZDARZYC)\n\n", onboard, Pcap);
    }

    g_st->bus_at_station = 0;
    free_before = g_st->buses_free;
    sem_unlock(g_semid);

    if(should_print(kurs, KIER_PRINT_EVERY)){
        printf("[KIER] ODJAZD kurs=%d onboard=%d bikes=%d free_pool=%d\n", kurs, onboard, bikes, free_before);
    }

    sem_lock(g_semid);
    int free_now = g_st->buses_free;
    sem_unlock(g_semid);

    if(free_now > 0){
        sem_lock(g_semid);
        g_st->buses_free -= 1;
        sem_unlock(g_semid);
        station_bus_ready();
    } else {
        station_empty();
    }

#if SLEEPY
    // DEMO
    int Ti_min = g_st->Ti_min;
    int Ti_max = g_st->Ti_max;
    int Ti = rand_in_range(Ti_min, Ti_max);
    in_trip = 1;
    alarm((unsigned)Ti);
#else
    sem_lock(g_semid);
    if(g_st->buses_total > 0) 
        g_st->buses_free += 1;
    int free_after = g_st->buses_free;
    int bus_here = g_st->bus_at_station;
    int kurs_now = (int)g_st->kurs;
    sem_unlock(g_semid);

    in_trip = 0;

    if(!bus_here && free_after > 0){
        sem_lock(g_semid);
        g_st->buses_free -= 1;
        sem_unlock(g_semid);
        station_bus_ready();
    }

    sem_lock(g_semid);
    if(g_st->bus_at_station && g_st->event == EV_ODJAZD) 
        g_st->event = EV_NONE;
    sem_unlock(g_semid);

    if(should_print(kurs_now, KIER_PRINT_EVERY)){
        printf("[KIER] POWROT free_pool=%d\n", free_after);
    }

    if(stop_req){
        sem_lock(g_semid);
        g_st->stop  = 1;
        g_st->event = EV_KONIEC;
        sem_unlock(g_semid);

        (void)kasa_send_stop_both(g_st->kasa_qid_vip, g_st->kasa_qid_norm);
        if(g_st->log_qid != -1) 
            (void)log_send_stop(g_st->log_qid);

        printf("[KIER] STOP (po powrocie)\n");
    }
#endif
}

int main(int argc, char** argv){
    if(argc < 5){
        fprintf(stderr, "usage: %s <shmid> <T> <Ti_min> <Ti_max>\n", argv[0]);
        return 1;
    }

    int shmid = atoi(argv[1]);
    int T = atoi(argv[2]);
    int Ti_min = atoi(argv[3]);
    int Ti_max = atoi(argv[4]);

    if(T <= 0) 
        T = 1;
    if(Ti_min <= 0) 
        Ti_min = T;
    if(Ti_max <= 0) 
        Ti_max = 3*T;

    g_st = shm_attach(shmid);
    g_semid = g_st->semid;

    sem_lock(g_semid);
    g_st->pid_kier = getpid();
    g_st->T = T;
    g_st->Ti_min = Ti_min;
    g_st->Ti_max = Ti_max;
    sem_unlock(g_semid);

    srand((unsigned)(time(NULL) ^ getpid()));

    struct sigaction sa1 = {0}, sa2 = {0};
    sa1.sa_handler = h_usr1;
    sa2.sa_handler = h_usr2;
    sigemptyset(&sa1.sa_mask);
    sigemptyset(&sa2.sa_mask);
    sa1.sa_flags = SA_RESTART;
    sa2.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa1, NULL);
    sigaction(SIGUSR2, &sa2, NULL);

#if SLEEPY
    struct sigaction saA = {0};
    saA.sa_handler = h_alrm;
    sigemptyset(&saA.sa_mask);
    saA.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &saA, NULL);
#endif
    sem_lock(g_semid);
    int N = g_st->buses_total;
    sem_unlock(g_semid);

    if(N >= 1) 
        station_bus_ready();
    else
        station_empty();

    sigset_t block, oldmask, waitmask;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    sigaddset(&block, SIGUSR2);
#if SLEEPY
    sigaddset(&block, SIGALRM);
#endif

    if(sigprocmask(SIG_BLOCK, &block, &oldmask) == -1){
        perror("sigprocmask");
        shm_detach(g_st);
        return 1;
    }

    waitmask = oldmask;
    sigdelset(&waitmask, SIGUSR1);
    sigdelset(&waitmask, SIGUSR2);
#if SLEEPY
    sigdelset(&waitmask, SIGALRM);
#endif
    sigdelset(&waitmask, SIGINT);
    sigdelset(&waitmask, SIGTERM);

    for(;;){
        while(!f_usr1 && !f_usr2
#if SLEEPY
              && !f_alrm
#endif
        ){
            sigsuspend(&waitmask);
        }

        if(f_usr2){
            f_usr2 = 0;
            stop_req = 1;

            if(!in_trip){
                sem_lock(g_semid);
                g_st->stop = 1;
                g_st->event = EV_KONIEC;
                sem_unlock(g_semid);

                (void)kasa_send_stop_both(g_st->kasa_qid_vip, g_st->kasa_qid_norm);
                if(g_st->log_qid != -1) 
                    (void)log_send_stop(g_st->log_qid);

                printf("[KIER] STOP (koniec)\n");
                break;
            }
        }

#if SLEEPY
        if(f_alrm){
            f_alrm = 0;
            demo_return_after_trip();
        }
#endif
        if(f_usr1){
            f_usr1 = 0;

            int ev, bus_here, do_force;

            sem_lock(g_semid);
            ev = (int)g_st->event;
            bus_here = g_st->bus_at_station;
            do_force = g_st->force_depart;

            if(bus_here && do_force){
                g_st->force_depart = 0;
            }
            sem_unlock(g_semid);

            if(ev == EV_KONIEC) 
                break;
            if(bus_here && do_force){
                do_depart();
            }
        }

#if !SLEEPY
        int bus_here, ev, onboard, P;
        sem_lock(g_semid);
        bus_here = g_st->bus_at_station;
        ev = (int)g_st->event;
        onboard = g_st->onboard;
        P = g_st->P;
        sem_unlock(g_semid);

        if(ev == EV_KONIEC) 
            break;

        if(bus_here && ev != EV_ODJAZD && onboard >= P){
            do_depart();
        }
#endif

        sem_lock(g_semid);
        int ev3 = (int)g_st->event;
        sem_unlock(g_semid);
        if(ev3 == EV_KONIEC) 
            break;
    }

    (void)sigprocmask(SIG_SETMASK, &oldmask, NULL);
    shm_detach(g_st);
    return 0;
}