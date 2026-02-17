#define _POSIX_C_SOURCE 200809L
#include "log.h"
#include "common.h"
#include "ipc.h"
#include "ring.h"
#include "pasazer.h"
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/sem.h>

//PASAZER - jedna osoba=jeden proces, losuje cechy (wiek, vip, rower, wchodzi do srodka, normalny idzie do kasy, potem wszyscy trfiaja do kolejki
//boarding i czekaja na zaproszenie od kierowcy

static volatile sig_atomic_t g_stop = 0; //flaga konczymy=1
static void on_sig(int sig){
    (void)sig;
    g_stop = 1;
}

static void install_handlers(void){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); //zerowanie smieci
    sa.sa_handler = on_sig; //podlaczamy handler
    sigemptyset(&sa.sa_mask); //brak blokowanych sygnalow w trak handlera
    sa.sa_flags = 0; //brak jakis trybow
    (void)sigaction(SIGINT,  &sa, NULL); //ctrl c i zabij proces
    (void)sigaction(SIGTERM, &sa, NULL);
}

//czy ipc umarlo
static int ipc_dead(void){
    return (errno == EIDRM || errno == EINVAL);
}

//czy dysp ustawil stop
static int shm_stop_now(passenger_args_t *a){
    if (!a->st)
        return 0; //brak shm nie ma jak czytac flagi
    if (sem_trydown(a->semid, SEM_SHM_MUTEX) == -1) //probujemy wejsc bez czekania - szybki check
        return 0;
    int v = a->st->stop; //odczyt flagi
    (void)sem_up(a->semid, SEM_SHM_MUTEX); //zwalniamy mutex
    return v;
}
//losowanie wieku
static int rand_age_1_80(unsigned *seed) {
    return (int)(rand_r(seed) % 80u) + 1;
}
//sem up/down
static int sem_down_retry(int semid, int idx){
    for(;;){
        if (sem_down(semid, idx) == 0) 
            return 0;
        if (errno == EINTR) {if (g_stop) return -1; continue; } //jesli przerwalo sie sygnalem - konczymy
        return -1;
    }
}
static int sem_up_retry(int semid, int idx){
    for(;;){ //^^^
        if (sem_up(semid, idx) == 0) return 0;
        if (errno == EINTR) { if (g_stop) return -1; continue; }
        return -1;
    }
}
//brak wyscigow //atomowosc - nie ma sytuacji ze ktos juz zlapal gate, nie odjal miejsca, a drugi robi balagan
static int enter_inside_atomic(int semid, int sem_gate, int sem_in, int seats){
    struct sembuf ops[2];
    ops[0].sem_num = (unsigned short)sem_gate; //bierzemy gate (0/1)
    ops[0].sem_op  = -1;
    ops[0].sem_flg = 0;
    ops[1].sem_num = (unsigned short)sem_in;
    ops[1].sem_op = (short)(-seats); //odejmujemy seats z sem in a/b
    ops[1].sem_flg = 0;

    for(;;){
        if (semop(semid, ops, 2) == 0) break; //albo wykonamy oba naraz albo zadne
        if (errno == EINTR) { if (g_stop) return -1; continue; }
        return -1;
    }

    if (sem_up_retry(semid, sem_gate) == -1) return -1; //puszczamy gate
    return 0;
}
//retry - probuja to samo, czyli probuj, jesliEINTR to retry albo stop, jesli IPC dead to wyjdz
//musimy obsluzyc wszystkich
static int ring_kasa_push_retry(ring_kasa_t *rk, int semid, const passenger_t *p){
    for(;;){
        if (ring_kasa_push(rk, semid, p) == 0) return 0;
        if (errno == EINTR) { if (g_stop) return -1; continue; }
        return -1;
    }
}
static int recv_kasa_resp_retry(int msg_kasa, pid_t me, int *ok){
    for(;;){
        if (msg_recv_kasa_resp(msg_kasa, me, ok) == 0) return 0;
        if (errno == EINTR) { if (g_stop) return -1; continue; }
        return -1;
    }
}
static int recv_invite_retry(int msg_invite, pid_t me, msg_invite_t *inv){
    for(;;){
        if (msg_recv_invite(msg_invite, me, inv) == 0) return 0;
        if (errno == EINTR) { if (g_stop) return -1; continue; }
        return -1;
    }
}

static int board_push_retry(passenger_args_t *a, const passenger_t *req){
    ring_board_t *rb = req->vip ? a->rb_vip : a->rb_norm;
    int sem_empty = req->vip ? SEM_BVIP_EMPTY : SEM_BNORM_EMPTY;
    int sem_full = req->vip ? SEM_BVIP_FULL : SEM_BNORM_FULL;
    int sem_mutex = req->vip ? SEM_BVIP_MUTEX : SEM_BNORM_MUTEX;

    for(;;){
        if (ring_board_push(rb, a->semid, req, sem_empty, sem_full, sem_mutex) == 0) return 0;
        if (errno == EINTR) { if (g_stop) return -1; continue; }
        return -1;
    }
}

int passenger_main(passenger_args_t a) {
    pid_t me; //pid procesu pasazera
    passenger_t req; //struktura pasazera wrzucana do ringu
    msg_invite_t inv; //zaproszenie od kierowcy
    int ok; //odpowiedz z kasy

    install_handlers(); //ustawianie sygnalow
    me = getpid(); //pobieranie pidu

    //losowanie, wybieranie
    unsigned seed = (unsigned)(time(NULL) ^ (unsigned)me ^ (unsigned)(a.no * 2654435761u)); 
    int age = rand_age_1_80(&seed);
    int is_child = (age < 8 ) ? 1 : 0;
    int seats = is_child ? 2 : 1;

    if (log_every_hit(a.no) || a.no == 1) { //nie logujemy wszystkich tylko co iles
        log_msg("PASAZER", "#%d start pid=%d age=%d child=%d seats=%d bike=%d vip=%d t=%d", a.no, (int)me, age, is_child, seats, a.bike, a.vip, sim_now());
    }

    int sem_in = a.bike ? SEM_IN_B : SEM_IN_A; //wybor wejsc
    int sem_gate = a.bike ? SEM_GATE_B : SEM_GATE_A;

    if (shm_stop_now(&a) || g_stop) return 0; //nie zaczynamy jest stop

    if (enter_inside_atomic(a.semid, sem_gate, sem_in, seats) == -1) { //atomowe wejscia //pas spi
        if (g_stop || ipc_dead()) return 0; //jesli IPC usuniete to 0
        log_msg("PASAZER", "#%d ERROR enter_inside_atomic errno=%d", a.no, errno); //blad =1
        return 1;
    }

    if (log_every_hit(a.no) || a.no == 1) {
        log_msg("PASAZER", "#%d WSZEDL (IN_%c -%d) t=%d", a.no, a.bike ? 'B' : 'A', seats, sim_now());
    }
    //dopisywanie, informacje dla kasy, boarding ring, kierowcy
    memset(&req, 0, sizeof(req));
    req.pid = me;
    req.passenger_no = a.no;
    req.vip = a.vip;
    req.bike = a.bike;
    req.age = age;
    req.is_child = is_child;
    req.seats = seats;

    if (!a.vip) { //normalny idzie do kasy
        if (ring_kasa_push_retry(a.rk, a.semid, &req) == -1) { //wrzuca ring do kasy
            if (g_stop || ipc_dead()) goto out_ok;
            log_msg("PASAZER", "#%d ERROR ring_kasa_push errno=%d", a.no, errno);
            goto out_err;
        }

        ok = 0;
        if (recv_kasa_resp_retry(a.msg_kasa, me, &ok) == -1) {
            if (g_stop || ipc_dead()) goto out_ok;
            log_msg("PASAZER", "#%d ERROR msgrcv resp errno=%d", a.no, errno);
            goto out_err;
        }
        (void)ok;
    } else { //vip
        if (log_every_hit(a.no) || a.no == 1) {
            log_msg("PASAZER", "#%d VIP omija kase t=%d", a.no, sim_now());
        }
    }
    //boarding
    if (board_push_retry(&a, &req) == -1) { //wrzucamy do ringa vip albo norm
        if (g_stop || ipc_dead()) goto out_ok;
        log_msg("PASAZER", "#%d ERROR board_push errno=%d", a.no, errno);
        goto out_err;
    }

    if (sem_up_retry(a.semid, SEM_BOARD_ANY) == -1) {
        if (g_stop || ipc_dead()) goto out_ok;
        log_msg("PASAZER", "#%d ERROR sem_up SEM_BOARD_ANY errno=%d", a.no, errno);
        goto out_err;
    }

    if (a.st) {
        if (sem_down_retry(a.semid, SEM_SHM_MUTEX) == 0) {
            a.st->arrived_total++; //dopiero jak pas jest w droku, wrzucil sie do boarding, dal token SEM_ANY
            (void)sem_up(a.semid, SEM_SHM_MUTEX);
        }
    }

    if (shm_stop_now(&a) || g_stop) goto out_ok;

    if (recv_invite_retry(a.msg_invite, me, &inv) == -1) { //odbior invite od kierowcy
        if (g_stop || ipc_dead()) goto out_ok;
        log_msg("PASAZER", "#%d ERROR msgrcv invite errno=%d", a.no, errno);
        goto out_err;
    }

    if (log_every_hit(a.no) || a.no == 1) {
        log_msg("PASAZER", "#%d INVITE k=%d kurs=%d age=%d child=%d seats=%d bike=%d vip=%d t=%d",
                a.no, inv.driver_id, inv.course_no, age, is_child, seats, inv.bike, inv.vip, sim_now());
    }

out_ok:
    for (int i = 0; i < seats; i++) (void)sem_up(a.semid, sem_in); //odanie miejsc sem in a/b
    return 0;

out_err:
    for (int i = 0; i < seats; i++) (void)sem_up(a.semid, sem_in);
    return 1;
}