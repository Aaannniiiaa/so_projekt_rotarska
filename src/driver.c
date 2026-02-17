#define _POSIX_C_SOURCE 200809L
#include "log.h"
#include "common.h"
#include "ipc.h"
#include "ring.h"
#include "driver.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>

//KIEROWCA - wybiera pasazerow do kursu z kolejki boarding (VIP najpierw), pilnuje limitow P i R, wysyla invite przez msg_invite do wybranego pas.

//flagi sygnalow
static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_force_depart = 0;

static void on_sigterm(int sig){
    (void)sig;
    g_stop = 1;
}
static void on_sigusr1(int sig){
    (void)sig;
    g_force_depart = 1;
}

static void install_handlers(void){
    struct sigaction sa;
    memset(&sa, 0,sizeof(sa)); //zerujemy zeby nie bylo smieci
    sigemptyset(&sa.sa_mask); //nie blokujemy dodatkowych sygnalow
    sa.sa_flags = 0; //brak innych opcji
    sa.sa_handler = on_sigterm;
    (void)sigaction(SIGINT, &sa, NULL); //dla ctrl c i sigterm handler ustawia stop
    (void)sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_sigusr1;
    (void)sigaction(SIGUSR1, &sa, NULL); //wymuszamy odjazd ale nie konczymy procesu
}
//ipc martwe
static int ipc_dead_errno(int e){
    return (e == EIDRM || e == EINVAL);
}
//czas teraz
static time_t now_s(void) { return time(NULL); }
//zwraca aktualny czas w sekundach //kurs trwa max T
static int remaining_s(time_t start_s, int window_s) { //oblicza ile sekund zostalo do konca okna T
    if (window_s <= 0) return 0; //jesli 0 - 0
    time_t end = start_s + (time_t)window_s; //start +T
    time_t now = now_s();
    return (end > now) ? (int)(end - now) : 0;
}
//szybki check stop, bez ryzyka zawisu
static int shm_stop_flag(driver_args_t *a) {
    if (sem_trydown(a->semid, SEM_SHM_MUTEX) == -1) { //probujemy wejsc bez blokowania
        if (ipc_dead_errno(errno)) return 1; //jesli ipc martwe - 1
        return 0; //jesli nie udalo sie wejsc - 0
    }
    int v = a->st->stop; //czytamy stop
    (void)sem_up(a->semid, SEM_SHM_MUTEX); //puszczamy mutex
    return v;
}
//
static int shm_read_launch_done(driver_args_t *a, int *out_done) {
    if (sem_trydown(a->semid, SEM_SHM_MUTEX) == -1) { //znowu probujemy wejsc
        if (ipc_dead_errno(errno)) return -2; //-2 - ipc dead
        return -1; //-1 nie udalo sie wejsc
    }
    int v = a->st->launch_done; //czytamy launch done
    (void)sem_up(a->semid, SEM_SHM_MUTEX); //puszczamy mutex
    if (out_done) *out_done = v; //zapisujemy do wskaznika jesli nie null
    return 0;
}

#ifndef DRIVER_LOG_EVERY_COURSE
#define DRIVER_LOG_EVERY_COURSE 200
#endif
#ifndef DRIVER_LOG_EVERY_SERVED
#define DRIVER_LOG_EVERY_SERVED 100
#endif
//logujemy kurs
static inline int should_log_course(int course) {
    return (DRIVER_LOG_EVERY_COURSE <= 1) || (course % DRIVER_LOG_EVERY_COURSE == 0) || (course == 1);
}
//logujemy obsluzonych
static inline int should_log_served(int served, int M) {
    if (served == 1) return 1;
    if (M > 0 && served == M) return 1;
    return (DRIVER_LOG_EVERY_SERVED > 0) && (served % DRIVER_LOG_EVERY_SERVED == 0);
}
//sem down zeby nie mielilo i zeby mozna bylo przerwac
static int sem_down_interruptible(int semid, int idx){
    for(;;){
        if (sem_down(semid, idx) == 0) return 0;
        if (errno == EINTR) {
            if (g_stop) return -1;
            continue;
        }
        return -1;
    }
}
//kolejka odlozonych
typedef struct {
    passenger_t *buf; //tablica pasazerow w pamieci
    int cap; //pojemnosc
    int sz; //ile w sro
    int head; //poczatek
} stash_t;
//tworzymy pusty magazyn o pojemnosci cap
static int stash_init(stash_t *s, int cap) {
    if (cap <= 0) cap = 1;
    s->buf = (passenger_t*)calloc((size_t)cap, sizeof(passenger_t)); //alokacja pamieci na cap pasazerow, calloc zeruje pamiec
    if (!s->buf) return -1; //brak pamieci - -1
    s->cap = cap;
    s->sz = 0;
    s->head = 0;
    return 0;
}
//sprzatanie
static void stash_free(stash_t *s) {
    free(s->buf);
    s->buf = NULL; //zwalniamy tablice buf
    s->cap = s->sz = s->head = 0; //czyscimy pola
}
//dodajemy pasazera na koniec kolejki stash
static int stash_push(stash_t *s, const passenger_t *p) {
    if (s->sz >= s->cap) return -1; //jak pelny - -1
    int idx = (s->head + s->sz) % s->cap; //head+sz - indeks ostatniego
    s->buf[idx] = *p; //kopiujemy pasazera do bufora
    s->sz++; //zwieksza rozmiar
    return 0;
}
//usuwamy element z wnetrza i przesuwa kolejke
static int stash_remove_at(stash_t *s, int k, passenger_t *out) {
    if (k < 0 || k >= s->sz) return 0; //jesli element spoza zakresu - 0
    int idx = (s->head + k) % s->cap;
    if (out) *out = s->buf[idx];
    for (int i = k; i < s->sz - 1; i++) { //przesuwamy elementy w lewo
        int from = (s->head + i + 1) % s->cap;
        int to = (s->head + i) % s->cap;
        s->buf[to] = s->buf[from];
    }
    s->sz--; //zmniejszamy size
    return 1; //udalo sie usunac
}
//bierzemy po kolejnosci przyjscia
static int stash_pop_bike_fifo(stash_t *s, passenger_t *out) {
    if (s->sz <= 0) return 0; //pusty - nie ma co brac
    *out = s->buf[s->head]; //bierzemy pierwszy element
    s->head = (s->head + 1) % s->cap; //przesuwamy head o 1
    s->sz--; //zmniejsza size
    return 1;
}
//bierzemy pierwszego, ktory pasuje do dostepnych miejsc, jak brak elementow albo miejsc to nic //zeby dziecko nie blokowalo
static int stash_pop_fit_ped(stash_t *s, int p_left, passenger_t *out) {
    if (s->sz <= 0 || p_left <= 0) return 0;
    for (int i = 0; i < s->sz; i++) { //lecimy po stashu
        int idx = (s->head + i) % s->cap;
        if (s->buf[idx].bike) continue; //jesli rower pomijamy 
        if (s->buf[idx].seats <= p_left) return stash_remove_at(s, i, out); //jak znajdziemy - usuwamy go z pozycji
    }
    return 0;
}
//probujemy wziac pasazera
static int board_try_take_one(driver_args_t *a, passenger_t *out) {
    if (sem_trydown(a->semid, SEM_BOARD_ANY) == -1) { //probujemy wziac bez czekania
        if (errno == EAGAIN) return 0;
        if (ipc_dead_errno(errno)) return -2;
        return -1;
    }

    int got = ring_board_try_pop(a->rb_vip, a->semid, out, SEM_BVIP_EMPTY, SEM_BVIP_FULL, SEM_BVIP_MUTEX); //probujemy wyjac z vip ringa
    //0 -nie ma, 1- wzieto, -1 - blad
    if (got == 1) return 1;
    if (got == -1) return -1;

    got = ring_board_try_pop(a->rb_norm, a->semid, out, SEM_BNORM_EMPTY, SEM_BNORM_FULL, SEM_BNORM_MUTEX); //jesli vipa nie bylo probujemy normal
    if (got == 1) return 1;
    if (got == -1) return -1;

    (void)sem_up(a->semid, SEM_BOARD_ANY); //jesli oba puste to pojawil sie moze jakis wyscig i oddajemy token
    return 0;
}
//wez jednego z poczekaniem
static int board_block_take_one(driver_args_t *a, passenger_t *out, int timeout_s_or_0_inf) {
    for(;;){
        if (g_stop) return -2;
        if (shm_stop_flag(a)) return -2;

        int r;
        if (timeout_s_or_0_inf == 0) r = sem_down(a->semid, SEM_BOARD_ANY); //czekamy na SEM BOARD ANY
        else r = sem_timeddown_s(a->semid, SEM_BOARD_ANY, timeout_s_or_0_inf); //czekamy tyle sekund
        //obsluga bledow
        if (r == -1) {
            if (errno == EINTR) {
                if (g_stop) return -2;
                continue;
            }
            if (errno == EAGAIN) return 0;
            if (ipc_dead_errno(errno)) return -2;
            return -1;
        }
        //znowu sprawdzamy stop
        if (g_stop || shm_stop_flag(a)) return -2;

        int got = ring_board_try_pop(a->rb_vip, a->semid, out, SEM_BVIP_EMPTY, SEM_BVIP_FULL, SEM_BVIP_MUTEX);
        if (got == 1) return 1;
        if (got == -1) return -1;

        got = ring_board_try_pop(a->rb_norm, a->semid, out, SEM_BNORM_EMPTY, SEM_BNORM_FULL, SEM_BNORM_MUTEX);
        if (got == 1) return 1;
        if (got == -1) return -1;

        (void)sem_up(a->semid, SEM_BOARD_ANY);
    }
}

int driver_main(driver_args_t a) {
    int course = 1; //numer kursu
    unsigned seed = (unsigned)(now_s() ^ (getpid() << 16) ^ (unsigned)a.driver_id); //do losowan

    stash_t stash_ped, stash_bike; //bufory kierowcy zeby nie gubic jak zabraknie miejsca
    install_handlers();

    if (stash_init(&stash_ped, a.M > 0 ? a.M : 1) == -1) return 1;
    if (stash_init(&stash_bike, a.M > 0 ? a.M : 1) == -1) { stash_free(&stash_ped); return 1; }

    log_msg("KIEROWCA", "%d START t=%d", a.driver_id, sim_now());

    for (;;) {
        //start kursu, limity miejsc
        time_t start = now_s();
        int p_left = a.P;
        int r_left = a.R;
        int log_course = should_log_course(course);
        g_force_depart = 0;

        if (g_stop || shm_stop_flag(&a)) { //jesli stop - sprzatamy i wychodzimy
            log_msg("KIEROWCA", "%d STOP t=%d", a.driver_id, sim_now());
            stash_free(&stash_ped);
            stash_free(&stash_bike);
            return 0;
        }

        if (log_course) log_msg("KIEROWCA", "%d KURS=%d START T=%d t=%d", a.driver_id, course, a.T, sim_now());

        while ((p_left > 0 || r_left > 0) && !g_stop) {
            passenger_t p;
            int got = 0;
            //warunki przerwania kursu
            if (shm_stop_flag(&a)) { g_stop = 1; break; } //stop
            if (g_force_depart) break; //sigusr1
            if (a.T > 0) { //limit czasu
                int rem0 = remaining_s(start, a.T);
                if (rem0 <= 0) break;
            }

            int want_ped = (p_left > 0);
            int want_bike = (r_left > 0);

            //najpierw probujemy ze stash zeby wykorzystac odlozonych
            if (want_ped && stash_pop_fit_ped(&stash_ped, p_left, &p)) got = 1;
            else if (want_bike && stash_pop_bike_fifo(&stash_bike, &p)) got = 1;

            //jesli nie ze stash to z boeardingu
            if (!got) {
                //T=0   
                if (a.T == 0) { //nie mamy okna czasowe, wiec jedziemy az braknie ludzi lub stop
                    int rr = board_try_take_one(&a, &p); //jesli nie ma - sprawdzamy launcz done i czy juz naprawde koniec
                    if (rr == 1) got = 1;
                    else if (rr == -2) { g_stop = 1; break; }
                    else if (rr == -1) return 1;

                    if (!got) {
                        int ld = 0;
                        int ok = shm_read_launch_done(&a, &ld);
                        if (ok == -2) {
                            g_stop = 1;
                            break;
                        }

                        if (ld) {
                            int any_tok = sem_getval(a.semid, SEM_BOARD_ANY);
                            int vip_full = sem_getval(a.semid, SEM_BVIP_FULL);
                            int nor_full = sem_getval(a.semid, SEM_BNORM_FULL);

                            if (any_tok <= 0 && vip_full <= 0 && nor_full <= 0 && stash_ped.sz == 0 && stash_bike.sz == 0) {
                                break; //juz nikt nie przyjdzie - break odjazd
                            }
                        }

                        rr = board_block_take_one(&a, &p, 0); //jesli nie jest pewne to czekamy az ktos sie pojawi
                        if (rr == 1) got = 1;
                        else if (rr == -2) {
                            g_stop = 1;
                            break;
                        }
                        else if (rr == -1) return 1;
                    }
                } else {
                    //T>0
                    int rem = remaining_s(start, a.T);
                    if (rem <= 0) break;

                    int rr = board_block_take_one(&a, &p, rem); //jak T minelo, koniec dobierania i odjazd
                    if (rr == 0) break;
                    if (rr == -2) {
                        g_stop = 1;
                        break;
                    }
                    if (rr == -1) return 1;
                    got = 1;
                }
            }
            if (!got) break;

            if (p.pid == 0 && p.passenger_no == -1) { //stop token, kierowca konczy proces
                stash_free(&stash_ped);
                stash_free(&stash_bike);
                log_msg("KIEROWCA", "%d STOP token -> KONIEC t=%d", a.driver_id, sim_now());
                return 0;
            }
            //limity miejsc
            //jak kogos wyciagnelismy ale nie mamy miejsca - odkladamy i konczymy petle, bo kierowca nie bedzie mieszal w nieskonoczosc, tylko jedzie
            if (p.bike) {
                if (r_left <= 0) {
                    (void)stash_push(&stash_bike, &p);
                    break;
                }
                r_left--;
            } else {
                if (p_left < p.seats) {
                    (void)stash_push(&stash_ped, &p);
                    break;
                }
                p_left -= p.seats;
            }

            { //invite do pasazera //pasazer czeka na msgrcv po mtype=pid
                msg_invite_t inv;
                inv.mtype = (long)p.pid;
                inv.ok = 1;
                inv.driver_id = a.driver_id;
                inv.course_no = course;
                inv.bike = p.bike;
                inv.vip = p.vip;

                if (msg_send_invite(a.msg_invite, p.pid, &inv) == -1) {
                    if (errno == EINTR && g_stop) break;
                    if (ipc_dead_errno(errno)) {
                        g_stop = 1;
                        break;
                    }
                    return 1;
                }
            }

            if (sem_down(a.semid, SEM_SHM_MUTEX) == -1) {
                if (errno == EINTR) continue;
                if (ipc_dead_errno(errno)) { g_stop = 1; break; }
                return 1;
            }
            a.st->served_total++; //licznik obsluzonych, mutex chroni
            int served = a.st->served_total;
            (void)sem_up(a.semid, SEM_SHM_MUTEX);

            if (should_log_served(served, a.M)) {
                log_msg("KIEROWCA","%d wybral #%d pid=%d age=%d child=%d seats=%d bike=%d vip=%d | P_left=%d R_left=%d served=%d/%d t=%d",
                a.driver_id, p.passenger_no, (int)p.pid, p.age, p.is_child, p.seats, p.bike, p.vip,p_left, r_left, served, a.M, sim_now());
            }
        }

        if (g_stop || shm_stop_flag(&a)) {
            log_msg("KIEROWCA", "%d KONIEC (stop) t=%d", a.driver_id, sim_now());
            stash_free(&stash_ped);
            stash_free(&stash_bike);
            return 0;
        }
        int gotA = 0, gotB = 0;
        //zamykamy na moment braki, loguje odjazd i je puszcza
        if (!g_stop && !shm_stop_flag(&a)) {
            if (sem_down_interruptible(a.semid, SEM_GATE_A) == 0) gotA = 1;
            if (sem_down_interruptible(a.semid, SEM_GATE_B) == 0) gotB = 1;
        }
        if (log_course) log_msg("KIEROWCA", "%d KURS=%d ODJAZD t=%d", a.driver_id, course, sim_now());
        if (gotB) (void)sem_up(a.semid, SEM_GATE_B);
        if (gotA) (void)sem_up(a.semid, SEM_GATE_A);

        if (a.Ti > 0) { //powrot
            int ti = (int)(rand_r(&seed) % (unsigned)a.Ti) + 1;
            if (log_course) log_msg("KIEROWCA", "%d KURS=%d POWROT Ti=%d (kernel wait) t=%d",a.driver_id, course, ti, sim_now());

            if (sem_timeddown_s(a.semid, SEM_DELAY, ti) == -1) { //spimy na semaforze SEM DELAY do timeouto
                if (errno != EAGAIN && errno != EINTR) {
                    if (ipc_dead_errno(errno)) { g_stop = 1; }
                    else return 1;
                }
            }
        } else {
            if (log_course) log_msg("KIEROWCA", "%d KURS=%d POWROT Ti=0 t=%d", a.driver_id, course, sim_now());
        }
        course++; //koniec kursu
    }

    stash_free(&stash_ped);
    stash_free(&stash_bike);
    return 0;
}