#define _POSIX_C_SOURCE 200809L
#include "dyspozytor.h"
#include "log.h"
#include "common.h"
#include "ipc.h"
#include "ring.h"
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

//DYSPOZYTOR - tworzy IPC (SHM+semafory+2 kolejki msg), odpala kasę i kierowców, a potem spawnuje pasażerów

//jak uruchomic program
static void usage(void) {
    write(2, "Uzycie: ./dyspozytor [N] [M] [INSIDE] [P] [R] [T] [Ti]\n", 61);
}

//flagi sygnalow
//zapis/odczyt atomowy, volatile - moze sie zmienic nagle 
static volatile sig_atomic_t g_hard_stop_req = 0; //ctrl+C/sigterm - konczymy ostro
static volatile sig_atomic_t g_soft_stop_req = 0; //konczymy miekko - SIGUSR2
static volatile sig_atomic_t g_depart_req = 0; //kazd kierowcom odjechac natychmiast SIGUSR1

//globalne zasoby IPC (kolejki, shm, sem)
//kolejki
static int g_msg_kasa = -1; //-1 -jeszcze nieuwotrzone //pasazer-kasa (odpowiedz ok)
static int g_msg_invite = -1; //kierowca-pasazer (wybralem cie do kursu)
//shm
static int g_shm_id = -1; //id segmentu shm
static void * g_base = (void*)0; //adres w pamieci
static size_t g_shm_sz = 0; //rozmiar shm (zeby wiedziec ile zaalokowano)
static int g_semid = -1; //id zestawu semaforow, w common mamy enum z indeksami semaforow
//wskazniki do obiektow w shm
static shared_state_t *g_st = NULL; //dysp robi shmat(), rozpakowywuje i zapisuje adresy do tego
static ring_kasa_t *g_rk = NULL;
static ring_board_t *g_rb_vip = NULL;
static ring_board_t *g_rb_norm = NULL;
static int g_N = 0; //liczba kierowcow //uzywamy w cleanupie i helperach
static int g_M = 0; //liczba pasazerow
//zeby moc wyslac sygnal, umiec rozpoznac kto zakonczyl sie w waitpid
static pid_t g_kasa_pid = -1; //pid procesu kasy
static pid_t *g_drivers = NULL; //tablica pid kierowcow (romiar N)
static pid_t *g_pids = NULL; //tablica pid pasazerow (rozmiar M)

//hadnler sygnalu
static void on_sig(int sig) {
    if (sig == SIGUSR1) {
        g_depart_req = 1; //jesli SIGURS1 - depart=1
        return;
    }
    if (sig == SIGUSR2) {
        g_soft_stop_req = 1; //jesli SIGUSR2 - soft_stop = 1
        return;
    }
    g_hard_stop_req = 1; //w innym przypadku hard_stop=1
}


static int install_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sig;
    sigemptyset(&sa.sa_mask); //jakie sygnaly zablokowac na czas dzialania handlera
    sa.sa_flags = 0; //bez innych zachowan

    if (sigaction(SIGINT, &sa, NULL) == -1)
        return -1; //jakby system odmowil ustawienia handlera
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        return -1;
    if (sigaction(SIGUSR2, &sa, NULL) == -1)
        return -1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
        return -1;
    return 0;
}

//SIGURS1 do kierowcow
static void forward_depart_to_drivers(void) {
    if (!g_drivers)
        return;
    for (int i = 0; i < g_N; i++) {
        if (g_drivers[i] > 0)
            (void)kill(g_drivers[i], SIGUSR1); //kill(pid, sygnal) - wysyla sygnal do kontretnego procesu, depart=1
    }
}

//procedura konczenia, zeby procesy nie wisialy na semaforach (driver), ringach (kasa), msg queue (pas)
static void request_stop_tokens(void) {
    if (g_semid == -1) //jesli nie istnieja, nie prpbujemy
        return;

    if (g_st) { //ustawiamy flagi w SHM
        if (sem_trydown(g_semid, SEM_SHM_MUTEX) == 0) {
            g_st->stop = 1; //konczymy
            g_st->launch_done = 1; //juz nie towrzymy, konczymy 
            (void)sem_up(g_semid, SEM_SHM_MUTEX);
        }
    }

    //falszywy pas stop
    passenger_t stopk;
    memset(&stopk, 0, sizeof(stopk));
    stopk.pid = 0; //stop token vv
    stopk.passenger_no = -1;

    if (g_rk)
        (void)ring_kasa_try_push(g_rk, g_semid, &stopk); //probujemy wrzucic stop do kasy, bo kasa siedzi w ring_kasa_pop i czeka na full
        //jesli dysp konczy to kasa ma dostac stop i wyjsc

    for (int i = 0; i < g_N; i++) {
        if (g_rb_vip) {
            (void)ring_board_try_push(g_rb_vip, g_semid, &stopk, SEM_BVIP_EMPTY, SEM_BVIP_FULL, SEM_BVIP_MUTEX); //proba wrzucenia stop do borad ring
        }
        (void)sem_up(g_semid, SEM_BOARD_ANY); //token budzacy kierowce, bo czesto czeka na sem_down(SEM_BOARD_ANY)
    }
}

//twardy reset zasobow systemowych
//zeby po zakonczeniu programu nie zostaly smieci IPC w systemie
static void hard_break_ipc(void) {
    if (g_msg_kasa != -1)   {
        (void)msgctl(g_msg_kasa, IPC_RMID, NULL); //usuwa kolejke komunikatow
        g_msg_kasa = -1;
    }
    if (g_msg_invite != -1) {
        (void)msgctl(g_msg_invite, IPC_RMID, NULL);
        g_msg_invite = -1;
    }

    if (g_semid != -1) {
        (void)semctl(g_semid, 0, IPC_RMID); //usuwa zestaw semaforow
        g_semid = -1;
    }
    if (g_shm_id != -1) {
        (void)shmctl(g_shm_id, IPC_RMID, NULL); //usuwa segment pamiedzi dzielonej
        g_shm_id = -1;
    }
}

//daj SIGTERM wszystkim - zakoczenie procesow, wysylamy do pasazerow, kasy, kierowcoe
static void signal_children_stop(void) {
    if (g_pids) {
        for (int i = 0; i < g_M; i++)
            if (g_pids[i] > 0)
                (void)kill(g_pids[i], SIGTERM);
    }
    if (g_kasa_pid > 0)
        (void)kill(g_kasa_pid, SIGTERM);

    if (g_drivers) {
        for (int i = 0; i < g_N; i++)
            if (g_drivers[i] > 0)
                (void)kill(g_drivers[i], SIGTERM);
    }
}

//sprzatanie zombie
static void wait_all_children(void) {
    for (;;) {
        pid_t w = waitpid(-1, NULL, 0); //czekamy na dowolne dziecko
        if (w > 0)
            continue;
        if (w == -1 && errno == EINTR)
            continue;
        break;
    }
}

//sprzatanie, zwykle wolane na koniec programu
static void cleanup_all(void) {
    //budzimy wszystko co moze wisiec
    request_stop_tokens();
    signal_children_stop(); //wysylamy sigterm dzieciom
    hard_break_ipc(); //usuwamy IPC
    wait_all_children(); //czekamy na dzieci

    if (g_base && g_base != (void*)-1) {
            (void)shmdt(g_base); //odlaczamy shm w dysp
            g_base = (void*)0;
    }

    //zwalniamt pamiec tablic pid
    free(g_pids);
    g_pids = NULL;
    free(g_drivers);
    g_drivers = NULL;
}

//sprawdza czy pid jest jednym z kierowcow
static int is_driver_pid(pid_t pid) {
    if (!g_drivers)
        return 0;
    for (int i = 0; i < g_N; i++) 
        if (g_drivers[i] == pid)
            return 1; //tak to kierowca
    return 0;
}

//jesli zakonczyl sie pasazet to go znajduje i ustawia 0 - zeby nie wyslac sygnalu do niezyjacego
static int mark_passenger_reaped(pid_t pid) {
    if (!g_pids)
        return 0;
    for (int i = 0; i < g_M; i++) {
        if (g_pids[i] == pid) {
            g_pids[i] = 0;
            return 1;
        }
    }
    return 0;
}

//zbieranie zakonczonych dzieci
static int reap_one(int *pass_done, int *pass_active) {
    int st; //status zakonczenia procesu
    pid_t pid; //pid procesu ktory sie zakonczyl

    for (;;) {
        pid = waitpid(-1, &st, 0); //czekamy na dowolne dziecko
        if (pid == -1) {
            if (errno == EINTR) { //przerwanie na sygnal
                if (g_soft_stop_req || g_hard_stop_req)
                    return 1;
                continue;
            }
            return -1;
        }
        break;
    }

    if (pid == g_kasa_pid) { //jesli kasa padla to konczymy
            g_kasa_pid = 0;
            g_hard_stop_req = 1;
            return 0;
    }
    if (is_driver_pid(pid)) { //jesli driver padl konczymy 
        g_hard_stop_req = 1;
        return 0;
    }

    (void)mark_passenger_reaped(pid); //oznacvzamy pid jako nieaktywny

    //jesli to pasazer
    if (pass_done) (*pass_done)++; //ile juz skonczylo
    if (pass_active) {
        if (*pass_active > 0) (*pass_active)--;
        else *pass_active = 0;
    }

    return 0;
}

//skalowanie opoznien spawnu, zeby sie az tak nie dluzyla
static int spawn_delay_cap_ms(int M) {
    int maxd = SPAWN_MAX_DELAY_MS;
    if (M >= 2000) maxd = 5;
    else if (M >= 1000) maxd = 10;
    else if (M >= 300)  maxd = 30;
    if (maxd < 0) maxd = 0;
    return maxd;
}

//fork+exec z kontrola czy exec sie udal
static pid_t spawn_exec_checked(const char *path, char *const argv_exec[]) {
    int pfd[2];
    if (pipe(pfd) == -1) { //zeby dziecko moglo wyslac rodzicowi errno jesli execv sie wywali
        log_msg("ERROR", "pipe() failed errno=%d", errno);
        return -1;
    }

    int flags = fcntl(pfd[1], F_GETFD);
    if (flags != -1) (void)fcntl(pfd[1], F_SETFD, flags | FD_CLOEXEC); //jesli exec sie uda - pipe znika - ridzc czyta i dostaje EOF

    pid_t p = fork();
    if (p == -1) {
        log_msg("ERROR", "fork() failed errno=%d", errno); //jesli fork sie nie udal, zamykamy pipe i konczymy z bledem
        close(pfd[0]); close(pfd[1]);
        return -1;
    }

    if (p == 0) { //dziecko
        close(pfd[0]);
        execv(path, argv_exec); //proba uruchomienia nowego programu, jesli wrocil - nie udal sie 
        int e = errno;
        (void)write(pfd[1], &e, sizeof(e));
        close(pfd[1]); //wychodzimy natychmiast
        _exit(127);
    }

    close(pfd[1]);

    int e = 0;
    ssize_t r = read(pfd[0], &e, sizeof(e)); //rodzic czyta, jesli exec sie udal - read zwroci 0
    close(pfd[0]);

    if (r == 0) {
        return p;
    }

    //logowanie bledu
    if (r == sizeof(e)) {
        log_msg("ERROR", "execv(%s) failed errno=%d", path, e);
    } else {
        log_msg("ERROR", "execv(%s) failed (read err) errno=%d", path, errno);
    }

    (void)kill(p, SIGTERM); //konczymy dziecko dla pewnosci
    (void)waitpid(p, NULL, 0); //sprztamy
    return -1;
}

int dyspozytor_main(int argc, char **argv) {
    int N, M, INSIDE, P, R, T, Ti;
    int MAX_ACTIVE = 200; //limit zyjacych pasazerow 

    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", (long)time(NULL)); //aktualny czas zmieniamy na tekst
        setenv(SIM_ENV_START, buf, 1); //wszystkie procesu licza czas od tego samego startu
    }

    //ustawianie loggera
    log_set_path("sim.log"); //sciezka pliku
    log_reset_file(); //otwiera plik w w i zamyka, czyli czysci logi na start
    log_set_msgid(-1); //nie uzywamy kolejki do logowania

    //sygnaly+sprzatanie
    (void)install_handlers(); //obsluga sygnalow
    atexit(cleanup_all);

    //czytanie parametrow
    N = (argc > 1) ? atoi(argv[1]) : DEF_N;
    M = (argc > 2) ? atoi(argv[2]) : DEF_M;
    INSIDE = (argc > 3) ? atoi(argv[3]) : DEF_INSIDE;
    P = (argc > 4) ? atoi(argv[4]) : DEF_P;
    R = (argc > 5) ? atoi(argv[5]) : DEF_R;
    T = (argc > 6) ? atoi(argv[6]) : DEF_T;
    Ti = (argc > 7) ? atoi(argv[7]) : DEF_Ti;

    if (N <= 0 || M < 0 || INSIDE <= 0 || P < 0 || R < 0 || T < 0 || Ti < 0) {
        usage();
        return 1;
    }

    //dla funkcji pomocniczych
    g_N = N;
    g_M = M;

    if (MAX_ACTIVE < INSIDE) MAX_ACTIVE = INSIDE;
    if (MAX_ACTIVE < 50) MAX_ACTIVE = 50;

    //naglowek
    log_msg("DYSPOZYTOR","START N=%d M=%d INSIDE=%d P=%d R=%d T=%d Ti=%d MAX_ACTIVE=%d VIP_PCT=%d BIKE_PCT=%d SPAWN_MAX_DELAY_MS=%d t=%d",
    N, M, INSIDE, P, R, T, Ti, MAX_ACTIVE, VIP_PCT, BIKE_PCT, SPAWN_MAX_DELAY_MS, sim_now());

    //tworzenie kolejki
    g_msg_kasa = msgget(IPC_PRIVATE, 0600 | IPC_CREAT); //pasazer-kasa
    g_msg_invite = msgget(IPC_PRIVATE, 0600 | IPC_CREAT); //kierowca-pasazer
    if (g_msg_kasa == -1 || g_msg_invite == -1) return 1;

    g_shm_sz = sizeof(shared_state_t) + sizeof(ring_kasa_t) + sizeof(ring_board_t) + sizeof(ring_board_t); //ile bajt potrzeb w jednym wspol SHM
    g_shm_id = shmget(IPC_PRIVATE, g_shm_sz, 0600 | IPC_CREAT);
    if (g_shm_id == -1) return 1;

    g_base = shmat(g_shm_id, NULL, 0); //podlaczamy segment shm do przestrzeni procesu
    if (g_base == (void*)-1) {
        g_base = (void*)0; 
        return 1;
    }
    memset(g_base, 0, g_shm_sz); //zerujemy shm zeby zaczynalo sie od 0

    //ustawianie wskaznikow
    char *ptr = (char*)g_base; 
    g_st = (shared_state_t*)ptr;
    ptr += sizeof(shared_state_t);
    g_rk = (ring_kasa_t*)ptr;
    ptr += sizeof(ring_kasa_t);
    g_rb_vip = (ring_board_t*)ptr;
    ptr += sizeof(ring_board_t);
    g_rb_norm = (ring_board_t*)ptr;

    //tworzenie semaforow
    g_semid = semget(IPC_PRIVATE, SEM_COUNT_BASE, 0600 | IPC_CREAT); //tworzymy zestaw, SEM_COUNT_BASE - liczba sem. w zestawie
    if (g_semid == -1) return 1;

    if (sem_setval(g_semid, SEM_SHM_MUTEX, 1) == -1) return 1; //mutex do shm, 1-mutex wolny

    int IN_B = INSIDE / 4;
    int IN_A = INSIDE - IN_B;
    if (IN_B < 2) IN_B = 2;
    if (IN_A < 0) IN_A = 0;

    if (sem_setval(g_semid, SEM_IN_A, IN_A) == -1) return 1;
    if (sem_setval(g_semid, SEM_IN_B, IN_B) == -1) return 1;
    if (sem_setval(g_semid, SEM_GATE_A, 1) == -1) return 1;
    if (sem_setval(g_semid, SEM_GATE_B, 1) == -1) return 1;
    if (sem_setval(g_semid, SEM_KASA_EMPTY, RING_KASA_SIZE) == -1) return 1;
    if (sem_setval(g_semid, SEM_KASA_FULL,  0) == -1) return 1;
    if (sem_setval(g_semid, SEM_KASA_MUTEX, 1) == -1) return 1;
    if (sem_setval(g_semid, SEM_BVIP_EMPTY, RING_BOARD_SIZE) == -1) return 1;
    if (sem_setval(g_semid, SEM_BVIP_FULL,  0) == -1) return 1;
    if (sem_setval(g_semid, SEM_BVIP_MUTEX, 1) == -1) return 1;
    if (sem_setval(g_semid, SEM_BNORM_EMPTY, RING_BOARD_SIZE) == -1) return 1;
    if (sem_setval(g_semid, SEM_BNORM_FULL,  0) == -1) return 1;
    if (sem_setval(g_semid, SEM_BNORM_MUTEX, 1) == -1) return 1;
    if (sem_setval(g_semid, SEM_BOARD_ANY, 0) == -1) return 1;
    if (sem_setval(g_semid, SEM_DELAY, 0) == -1) return 1;
    if (sem_setval(g_semid, SEM_SPAWN_DELAY, 0) == -1) return 1;

    { //uruchomienie procesu kasa
        char shmid_s[32], semid_s[32], msg_s[32];
        snprintf(shmid_s, sizeof(shmid_s), "%d", g_shm_id);
        snprintf(semid_s, sizeof(semid_s), "%d", g_semid); //sem
        snprintf(msg_s,   sizeof(msg_s),   "%d", g_msg_kasa); //kolejka do odpowiedzi z kasy
        char *argv_kasa[] = {"./kasa", shmid_s, semid_s, msg_s, NULL}; //nazwa programu, id shm, id semaforow, id kolejki do kasy
        g_kasa_pid = spawn_exec_checked("./kasa", argv_kasa); //helper robi pipe, fork, execv, jesli sie nie uda wypisuje errrno
        if (g_kasa_pid < 0) return 1;
    }

    //uruchomienie kierowcow 
    g_drivers = (pid_t*)calloc((size_t)N, sizeof(pid_t)); //alokujemy tablice na pidy kierowcow zeby moc im wyslac pozniej sygnaly 
    if (!g_drivers) return 1;

    for (int i = 1; i <= N; i++) {
        char id_s[16], N_s[16], M_s[16], INS_s[16], P_s[16], R_s[16], T_s[16], Ti_s[16];
        char shmid_s[32], semid_s[32], msgi_s[32];

        //przekazujemy parametry
        snprintf(id_s, sizeof(id_s),"%d", i);
        snprintf(N_s, sizeof(N_s), "%d", N);
        snprintf(M_s, sizeof(M_s), "%d", M);
        snprintf(INS_s, sizeof(INS_s), "%d", INSIDE);
        snprintf(P_s, sizeof(P_s), "%d", P);
        snprintf(R_s, sizeof(R_s), "%d", R);
        snprintf(T_s, sizeof(T_s), "%d", T);
        snprintf(Ti_s, sizeof(Ti_s), "%d", Ti);
        snprintf(shmid_s, sizeof(shmid_s), "%d", g_shm_id);
        snprintf(semid_s, sizeof(semid_s), "%d", g_semid);
        snprintf(msgi_s,  sizeof(msgi_s),  "%d", g_msg_invite);

        char *argv_drv[] = {"./driver",id_s, N_s, M_s, INS_s, P_s, R_s, T_s, Ti_s,shmid_s, semid_s, msgi_s,NULL };
        pid_t k = spawn_exec_checked("./driver", argv_drv); //start, jak nie uda sie to blad
        if (k < 0) return 1;
        g_drivers[i-1] = k; //do np sygnalow
    }

    //pasazerowie uruchomienie
    g_pids = (pid_t*)calloc((size_t)M, sizeof(pid_t)); //tablica pidow pasazerow  zeby moc ich pozniej ubic
    if (!g_pids) return 1;

    int launched = 0, done = 0, active = 0; //ile juz uruchomilam, zakonczylo, zyje
    unsigned dseed_delay = (unsigned)(time(NULL) ^ (unsigned)getpid() ^ 0xA5A5u); //losuje opoznienie miedzy spawnami
    unsigned dseed_type = (unsigned)(time(NULL) ^ (unsigned)getpid() ^ 0x5A5Au); //lsuje VIP/bike
    int spawn_cap = spawn_delay_cap_ms(M); //maks delay miedzy spawnami 

    while (done < M) { //dopoki wszyscy nie skoncza
        if (g_depart_req) { //obsluga SIGURS1
            g_depart_req = 0;
            log_msg("DYSPOZYTOR", "SIGUSR1 -> forward do kierowcow t=%d", sim_now());
            forward_depart_to_drivers();
        }
        if (g_soft_stop_req) { //obsluga SIGURS2
            g_soft_stop_req = 0;
            log_msg("DYSPOZYTOR", "SIGUSR2 -> SOFT STOP t=%d", sim_now());
            request_stop_tokens();
            signal_children_stop();
            hard_break_ipc();
            wait_all_children();
            log_msg("DYSPOZYTOR", "SOFT STOP: zakonczono t=%d", sim_now());
            return 0;
        }
        if (g_hard_stop_req) {
            log_msg("DYSPOZYTOR", "HARD STOP t=%d", sim_now());
            request_stop_tokens();
            signal_children_stop();
            hard_break_ipc();
            wait_all_children();
            return 0;
        }

        if (launched < M && active < MAX_ACTIVE) {
            if (spawn_cap > 0) {
                int dms = (int)(rand_r(&dseed_delay) % (unsigned)(spawn_cap + 1)); //opoznienie spawn
                if (dms > 0) {
                    if (sem_timeddown_ms(g_semid, SEM_SPAWN_DELAY, dms) == -1) { //czekanie
                        if (errno != EAGAIN && errno != EINTR) {
                            log_msg("DYSPOZYTOR", "WARN: sem_timeddown_ms errno=%d", errno);
                        }
                    }
                }
            }

            //losowanie parametrow
            int no = launched + 1;
            int vip = ((int)(rand_r(&dseed_type) % 100u) < VIP_PCT) ? 1 : 0;
            int bike = ((int)(rand_r(&dseed_type) % 100u) < BIKE_PCT) ? 1 : 0;

            char no_s[16], vip_s[8], bike_s[8];
            char shmid_s[32], semid_s[32], msgk_s[32], msgi_s[32];
            snprintf(no_s, sizeof(no_s), "%d", no); //budowanie str
            snprintf(vip_s, sizeof(vip_s),"%d", vip);
            snprintf(bike_s, sizeof(bike_s), "%d", bike);
            snprintf(shmid_s, sizeof(shmid_s), "%d", g_shm_id);
            snprintf(semid_s, sizeof(semid_s), "%d", g_semid);
            snprintf(msgk_s, sizeof(msgk_s), "%d", g_msg_kasa);
            snprintf(msgi_s, sizeof(msgi_s), "%d", g_msg_invite);

            char *argv_pas[] = { "./pasazer", no_s, vip_s, bike_s, shmid_s, semid_s, msgk_s, msgi_s, NULL };
            pid_t p = spawn_exec_checked("./pasazer", argv_pas); //odpalanie i ochrona

            if (p > 0) {
                g_pids[launched++] = p; //jesli uskces zapisujemy pid do tablicy
                active++;
                if (launched == M && g_st) {
                    if (sem_trydown(g_semid, SEM_SHM_MUTEX) == 0) {
                        g_st->launch_done = 1;
                        (void)sem_up(g_semid, SEM_SHM_MUTEX);
                    }
                }
                continue;
            }
            //obsluga bledow
            if (errno == EAGAIN || errno == ENOMEM) {
                int rr = reap_one(&done, &active); //zbieramy dziecko i dalej
                if (rr == 1) continue;
                if (rr == -1) break;
                continue;
            }
            if (errno == EINTR) continue;

            log_msg("DYSPOZYTOR", "ERROR fork passenger errno=%d -> HARD STOP", errno);
            g_hard_stop_req = 1;
            continue;
        }

        if (active > 0) { //czekamy na zakonczenie dziecka 
            int rr = reap_one(&done, &active);
            if (rr == 1) continue;
            if (rr == -1) break;
        } else {
            log_msg("DYSPOZYTOR", "ERROR: active=0 a done=%d/M=%d -> HARD STOP", done, M);
            g_hard_stop_req = 1;
        }
    }

    log_msg("DYSPOZYTOR", "ALL passengers done -> koniec t=%d", sim_now());
    request_stop_tokens();
    signal_children_stop();
    hard_break_ipc();
    wait_all_children();
    log_msg("DYSPOZYTOR", "OK: zakonczono t=%d", sim_now());
    return 0;
}