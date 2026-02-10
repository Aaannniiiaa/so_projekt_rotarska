#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "kasa_ipc.h"
#include "log_ipc.h"
#include "sem_ipc.h"
#include "common.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SLEEPY
static int g_mode_demo = 1;
#else
static int g_mode_demo = 0;
#endif

static volatile sig_atomic_t g_tick = 0;
static volatile sig_atomic_t g_chld = 0;
static volatile sig_atomic_t g_stop = 0;

static pid_t g_pass_pgid = -1;

static int g_shmid = -1;
static int g_qvip = -1;
static int g_qnorm = -1;
static int g_log_qid = -1;
static int g_semid = -1;
static station_state* g_st = NULL;

static pid_t pid_logger = -1;
static pid_t pid_kasa = -1;
static pid_t pid_dysp = -1;
static pid_t pid_kier = -1;
static volatile sig_atomic_t g_alarm = 0;

static void h_alarm(int s){
    (void)s;
    g_alarm = 1;
}
static void h_alrm(int s){
    (void)s;
    g_tick = 1;
    g_alarm=1;
}
static void h_chld(int s){
    (void)s;
    g_chld = 1;
}
static void h_int (int s){
    (void)s;
    g_stop = 1;
}

static void stop_timer_demo_only(void){
#if SLEEPY
    if(!g_mode_demo)
        return;
    struct itimerval it;
    memset(&it, 0, sizeof(it));
    (void)setitimer(ITIMER_REAL, &it, NULL);
#else
    (void)0;
#endif
}

static pid_t spawn_exec(const char* path, char* const argv[]){
    pid_t p = fork();
    if(p == -1){
        perror("fork");
        exit(1);
    }
    if(p == 0){
        execv(path, argv);
        perror("execv");
        _exit(127);
    }
    return p;
}

static pid_t spawn_passenger(const char* path, char* const argv[]){
    pid_t p = fork();
    if(p == -1){
        perror("fork");
        exit(1);
    }

    if(p == 0){
        if(g_pass_pgid == -1) 
            setpgid(0, 0);
        else 
            setpgid(0, g_pass_pgid);
        execv(path, argv);
        perror("execv");
        _exit(127);
    }

    if(g_pass_pgid == -1){
        g_pass_pgid = p;
        (void)setpgid(p, g_pass_pgid);
    }else{
        (void)setpgid(p, g_pass_pgid);
    }
    return p;
}

static void stop_all_passengers(void){
    if(g_pass_pgid > 0){
        kill(-g_pass_pgid, SIGTERM);
    }
}

static void reap_children(int *active){
    for(;;){
        int status = 0;
        pid_t w = waitpid(-1, &status, WNOHANG);
        if(w <= 0) 
            break;

        if(w != pid_dysp && w != pid_kier && w != pid_kasa && w != pid_logger){
            if(*active > 0) 
                (*active)--;
        }
    }
}

static void cleanup_ipc_only(void){
    if(g_st){ 
        shm_detach(g_st);
        g_st = NULL;
    }
    if(g_qvip != -1){
        kasa_remove(g_qvip);
        g_qvip  = -1;
    }
    if(g_qnorm != -1){
        kasa_remove(g_qnorm);
        g_qnorm = -1;
    }
    if(g_log_qid != -1){
        log_remove(g_log_qid);
        g_log_qid  = -1;
    }
    if(g_shmid != -1){
        st_shm_remove(g_shmid);
        g_shmid = -1;
    }
    if(g_semid != -1){
        sem_remove(g_semid);
        g_semid = -1;
    }
}

static void hard_reset_shm(int N, int P, int R, int T, int Ti_min, int Ti_max){
    sem_lock(g_semid);

    g_st->event = EV_NONE;
    g_st->stop  = 0;
    g_st->kurs  = 0;
    g_st->force_depart = 0;
    g_st->next_ticket = 1000;
    g_st->cnt_req_total = 0;
    g_st->cnt_vip   = 0;
    g_st->cnt_child = 0;
    g_st->cnt_bike  = 0;
    g_st->last_ticket = 999;
    g_st->waiting_vip  = 0;
    g_st->waiting_norm = 0;
    g_st->P = P;
    g_st->R = R;
    g_st->onboard = 0;
    g_st->onboard_bikes = 0;
    g_st->buses_total = N;
    g_st->buses_free = (N > 0 ? N - 1 : 0);
    g_st->bus_at_station = (N > 0 ? 1 : 0);
    g_st->T = T;
    g_st->Ti_min = Ti_min;
    g_st->Ti_max = Ti_max;

    sem_unlock(g_semid);
}

static void wake_kasa_sems(void){
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
    (void)sem_do(g_semid, ops, 3);
}

static void request_stop_everywhere(void){
    if(!g_st) 
        return;

    sem_lock(g_semid);
    g_st->event = EV_KONIEC;
    g_st->stop = 1;
    g_st->waiting_vip  = 0;
    g_st->waiting_norm = 0;
    sem_unlock(g_semid);

    sem_lock(g_semid);
    pid_t pd = g_st->pid_dysp;
    pid_t pk = g_st->pid_kier;
    sem_unlock(g_semid);

    if(pd > 0) 
        kill(pd, SIGUSR2);
    if(pk > 0) 
        kill(pk, SIGUSR2);

    (void)kasa_send_stop_both(g_qvip, g_qnorm);
    wake_kasa_sems();

    if(g_log_qid != -1) 
        (void)log_send_stop(g_log_qid);

    stop_all_passengers();
}

static void spawn_more(int Mpass, int MAXF, int *spawned, int *active, const char *shmbuf, const char *vipbuf, const char *normbuf)
{
    while(*spawned < Mpass && *active < MAXF){
        sem_lock(g_semid);
        int ev = (int)g_st->event;
        int stop = g_st->stop;
        sem_unlock(g_semid);
        if(stop || ev == EV_KONIEC) 
            break;

        char ibuf[32];
        snprintf(ibuf, sizeof(ibuf), "%d", *spawned);

        char* argv_p[] = { (char*)"./passenger_sim", ibuf, (char*)shmbuf, (char*)vipbuf, (char*)normbuf, NULL };
        (void)spawn_passenger("./passenger_sim", argv_p);

        (*spawned)++;
        (*active)++;

#if SLEEPY
        SIM_USLEEP(2000);
#endif
    }
}

static void fast_maybe_depart_full(void){
    int bus_here, ev, onboard, P, force;
    sem_lock(g_semid);
    bus_here = g_st->bus_at_station;
    ev = (int)g_st->event;
    onboard = g_st->onboard;
    P = g_st->P;
    force = g_st->force_depart;
    sem_unlock(g_semid);

    if(bus_here && ev != EV_ODJAZD && onboard >= P && force == 0){
        if(pid_dysp > 0) 
            kill(pid_dysp, SIGUSR1);
    }
}

static int should_natural_end(int Mpass, int spawned, int active){
    int total, onboard, wv, wn, ev;

    sem_lock(g_semid);
    total = g_st->cnt_req_total;
    onboard = g_st->onboard;
    wv = g_st->waiting_vip;
    wn = g_st->waiting_norm;
    ev = (int)g_st->event;
    sem_unlock(g_semid);

    return (spawned >= Mpass && total >= Mpass && active == 0 && onboard == 0 && wv == 0 && wn == 0 && ev != EV_ODJAZD);
}

static int shm_requests_stop(void){
    int stop, ev;
    sem_lock(g_semid);
    stop = g_st->stop;
    ev = (int)g_st->event;
    sem_unlock(g_semid);
    return (stop || ev == EV_KONIEC);
}

static int kasa_done(int Mpass){
    int total;
    sem_lock(g_semid);
    total = g_st->cnt_req_total;
    sem_unlock(g_semid);
    return total >= Mpass;
}

static int is_gone(pid_t pid){
    if(pid <= 0) 
        return 1;
    if(kill(pid, 0) == -1){
        if(errno == ESRCH) 
            return 1;
    }
    return 0;
}

static void wait_workers_no_time_limit(void){
    for(;;){
        int st = 0;
        pid_t w = waitpid(-1, &st, 0);
        if(w == -1){
            if(errno == EINTR)
                continue;
            break;
        }
    }
    stop_all_passengers();
}

static void wait_workers_with_timeout(void){
   g_alarm = 0;
    alarm(5);

    for(;;){
        int st = 0;
        pid_t w = waitpid(-1, &st, 0);
        if(w == -1){
            if(errno == EINTR){
                if(g_alarm) 
                    break;
                continue;
            }
            break;
        }
    }

    alarm(0);

    if(!is_gone(pid_dysp))
        kill(pid_dysp, SIGKILL);
    if(!is_gone(pid_kier))
        kill(pid_kier, SIGKILL);
    if(!is_gone(pid_kasa))
        kill(pid_kasa, SIGKILL);
    if(!is_gone(pid_logger))
        kill(pid_logger, SIGKILL);
    stop_all_passengers();

    for(;;){
        int st=0;
        pid_t w = waitpid(-1, &st, WNOHANG);
        if(w <= 0)
            break;
    }
}

int main(int argc, char** argv){
    int Nbus = 4;
    int P = 20;
    int R = 4;
    int T = 1;
    int Mpass = 5000;
    int Ti_min = 1;
    int Ti_max = 3;

    if(argc == 1){
    }else if(argc >= 8){
        Nbus = atoi(argv[1]);
        P = atoi(argv[2]);
        R = atoi(argv[3]);
        T = atoi(argv[4]);
        Mpass = atoi(argv[5]);
        Ti_min = atoi(argv[6]);
        Ti_max = atoi(argv[7]);
        if(argc >= 9 && strcmp(argv[8], "demo") == 0)
            g_mode_demo = 1;
    }else {
        fprintf(stderr, "usage: %s <Nbus> <P> <R> <Tsec> <Mpass> <Ti_min> <Ti_max> [demo]\n", argv[0]);
        return 1;
    }

    if(Nbus <= 0) 
        Nbus = 1;
    if(P <= 0) 
        P = 1;
    if(R < 0)  
        R = 0;
    if(R > P)  
        R = P;
    if(T <= 0) 
        T = 1;
    if(Mpass < 0) 
        Mpass = 0;
    if(Ti_min <= 0) 
        Ti_min = T;
    if(Ti_max <= 0) 
        Ti_max = 3*T;

    int MAXF = g_mode_demo ? 500 : 400;
    if(MAXF > Mpass) 
        MAXF = Mpass;

    struct sigaction saI = {0}, saC = {0}, saA = {0};
    saI.sa_handler = h_int;
    sigemptyset(&saI.sa_mask);
    saI.sa_flags = 0;
    saC.sa_handler = h_chld;
    sigemptyset(&saC.sa_mask);
    saC.sa_flags = SA_RESTART;
    saA.sa_handler = h_alrm;
    sigemptyset(&saA.sa_mask);
    saA.sa_flags = SA_RESTART;

    sigaction(SIGINT, &saI, NULL);
    sigaction(SIGTERM, &saI, NULL);
    sigaction(SIGCHLD, &saC, NULL);
    sigaction(SIGALRM, &saA, NULL);

    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);

    int created_vip = 0, created_norm = 0, created_log = 0, created_sem = 0, created_shm = 0;
    g_qvip = kasa_vip_create_clean(&created_vip);
    g_qnorm = kasa_norm_create_clean(&created_norm);
    g_log_qid = log_create_clean(&created_log);
    g_semid = sem_create_clean(&created_sem);

    g_shmid = st_shm_create_clean(&created_shm);
    g_st = shm_attach(g_shmid);
    st_init_if_created(g_st, created_shm, g_shmid, g_qvip, g_qnorm, g_log_qid, g_semid);

    hard_reset_shm(Nbus, P, R, T, Ti_min, Ti_max);

    sem_setval(g_semid, SEM_BOARDING, 0);
    sem_setval(g_semid, SEM_QANY, (Nbus > 0 ? 0 : 1));

    if(Nbus > 0){
        sem_setval(g_semid, SEM_SEATS, P);
        sem_setval(g_semid, SEM_BIKES, R);
        sem_setval(g_semid, SEM_ENTR1, 1);
        sem_setval(g_semid, SEM_ENTR2, 1);
    } else {
        sem_setval(g_semid, SEM_SEATS, 0);
        sem_setval(g_semid, SEM_BIKES, 0);
        sem_setval(g_semid, SEM_ENTR1, 0);
        sem_setval(g_semid, SEM_ENTR2, 0);
    }

    char vipbuf[32], normbuf[32], log_qbuf[32], shmbuf[32];
    snprintf(vipbuf, sizeof(vipbuf), "%d", g_qvip);
    snprintf(normbuf, sizeof(normbuf), "%d", g_qnorm);
    snprintf(log_qbuf, sizeof(log_qbuf), "%d", g_log_qid);
    snprintf(shmbuf, sizeof(shmbuf), "%d", g_shmid);

    char* argv_log[] = { (char*)"./logger", log_qbuf, shmbuf, NULL };
    pid_logger = spawn_exec("./logger", argv_log);

    char* argv_kasa[] = { (char*)"./kasa", shmbuf, vipbuf, normbuf, log_qbuf, NULL };
    pid_kasa = spawn_exec("./kasa", argv_kasa);

    char* argv_d[] = { (char*)"./dyspozytor", shmbuf, vipbuf, normbuf, log_qbuf, NULL };
    pid_dysp = spawn_exec("./dyspozytor", argv_d);

    char Tbuf[32], Timinbuf[32], Timaxbuf[32];
    snprintf(Tbuf, sizeof(Tbuf), "%d", T);
    snprintf(Timinbuf, sizeof(Timinbuf), "%d", Ti_min);
    snprintf(Timaxbuf, sizeof(Timaxbuf), "%d", Ti_max);

    char* argv_k[] = { (char*)"./kierowca", shmbuf, Tbuf, Timinbuf, Timaxbuf, NULL };
    pid_kier = spawn_exec("./kierowca", argv_k);

    printf("[SIM] start | N=%d P=%d R=%d | T=%d | Ti=[%d..%d] | Mpass=%d | MAXF=%d | mode=%s\n", Nbus, P, R, T, Ti_min, Ti_max, Mpass, MAXF,
    g_mode_demo ? "demo" : "fast");
    printf("Terminal2: kill -USR2 %d (KONIEC)\n", (int)pid_dysp);
    fflush(stdout);

#if SLEEPY
    if(g_mode_demo){
        struct itimerval it;
        memset(&it, 0, sizeof(it));
        it.it_value.tv_sec = T;
        it.it_interval.tv_sec = T;
        if(setitimer(ITIMER_REAL, &it, NULL) == -1){
            perror("setitimer");
            cleanup_ipc_only();
            return 1;
        }
    }
#endif

    int spawned = 0, active = 0;
    int shutdown_sent = 0;

    sigset_t block, oldmask, waitmask;
    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
#if SLEEPY
    sigaddset(&block, SIGALRM);
#endif

    if(sigprocmask(SIG_BLOCK, &block, &oldmask) == -1){
        perror("sigprocmask");
        cleanup_ipc_only();
        return 1;
    }

    waitmask = oldmask;
    sigdelset(&waitmask, SIGCHLD);
#if SLEEPY
    sigdelset(&waitmask, SIGALRM);
#endif
    sigdelset(&waitmask, SIGINT);
    sigdelset(&waitmask, SIGTERM);

    spawn_more(Mpass, MAXF, &spawned, &active, shmbuf, vipbuf, normbuf);

    while(!shutdown_sent){
        if(shm_requests_stop()){
            stop_timer_demo_only();
            request_stop_everywhere();
            shutdown_sent = 1;
            break;
        }

        if(g_stop){
            stop_timer_demo_only();
            request_stop_everywhere();
            shutdown_sent = 1;
            break;
        }

        if(g_chld){
            g_chld = 0;
            reap_children(&active);
        } else {
            reap_children(&active);
        }

#if SLEEPY
        if(g_mode_demo && g_tick && !shutdown_sent){
            g_tick = 0;

            int bus_here, ev, onboard, wv, wn;
            sem_lock(g_semid);
            bus_here = g_st->bus_at_station;
            ev = (int)g_st->event;
            onboard = g_st->onboard;
            wv = g_st->waiting_vip;
            wn = g_st->waiting_norm;
            sem_unlock(g_semid);

            if(pid_dysp > 0 && bus_here && ev != EV_ODJAZD && (onboard > 0 || wv > 0 || wn > 0))
            {
                kill(pid_dysp, SIGUSR1);
            }
        }
#endif
        if(!g_mode_demo){
            fast_maybe_depart_full();
        }

        if(spawned < Mpass){
            spawn_more(Mpass, MAXF, &spawned, &active, shmbuf, vipbuf, normbuf);
        }

        if(should_natural_end(Mpass, spawned, active)){
            stop_timer_demo_only();
            request_stop_everywhere();
            shutdown_sent = 1;
            break;
        }

        if(!g_mode_demo && spawned >= Mpass && kasa_done(Mpass)){
            request_stop_everywhere();
            shutdown_sent = 1;
            break;
        }

        if(!shutdown_sent){
            if(active >= MAXF || (spawned >= Mpass && active > 0) || g_mode_demo){
                sigsuspend(&waitmask);
            }
        }
    }

    (void)sigprocmask(SIG_SETMASK, &oldmask, NULL);

    //wait_workers_with_timeout();
    wait_workers_no_time_limit();
    cleanup_ipc_only();

    printf("=== STOP ===\n");
    printf("OK (raport.txt)\n");
    return 0;
}