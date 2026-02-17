#define _POSIX_C_SOURCE 200809L
#include "log.h"
#include "common.h"
#include "ipc.h"
#include "ring.h"
#include "kasa.h"
#include <errno.h>
#include <signal.h>
#include <string.h>

//KASA - obsluguje normalnych pasazerow, odbiera ich z kolejki ring ring_kasa i odsyla im ok przez msg_kasa

//flagi
static volatile sig_atomic_t g_stop = 0;
static void on_sig(int sig){
    (void)sig;
    g_stop = 1;
}

static void install_handlers(void){
    struct sigaction sa; //tworzymy strukture
    memset(&sa, 0, sizeof(sa)); //zerujemy
    sa.sa_handler = on_sig; //handler na on sig
    sigemptyset(&sa.sa_mask); //maska pusta - nie blokujemy dodatkowych sygnalow
    sa.sa_flags = 0;
    (void)sigaction(SIGINT, &sa, NULL); //obsluga dla ctrl c i sigterm
    (void)sigaction(SIGTERM, &sa, NULL);
}

//wysylanie odpowiedzi do pasazera //retry zeby kasa nie wywalila sie przez sygnal
static int send_kasa_resp_retry(int msg_kasa, pid_t pid, int ok){
    for(;;){
        if (msg_send_kasa_resp(msg_kasa, pid, ok) == 0) return 0; //robi msgsng 
        if (errno == EINTR) { //moze sie przerwac sygnalem
            if (g_stop) return -1;
            continue; //wiec probujemy jeszcze raz
        }
        return -1;
    }
}

int kasa_main(int msg_kasa, int semid, ring_kasa_t *rk) {
    install_handlers();
    log_msg("KASA", "START t=%d", sim_now()); //loguje start z czasem symulacji

    for (;;) { //dziala dopiki nie dostanie stop
        passenger_t p;
        if (ring_kasa_pop(rk, semid, &p) == -1) { //blokujace pobranie z kolejki ring: czeka na SEM KASA FULL, lockuje SEM KASA MUTEX, pobniera ele
            //oblokowuje mutex i podbija SEM KASA EMPTY - zwalnia miejsce
            //obsluga bledu
            if (g_stop) break; //koniec
            if (errno == EINTR) continue;
            if (errno == EIDRM || errno == EINVAL) break; //ipc zniknelo konczymy
            log_msg("KASA", "ERROR ring_kasa_pop errno=%d", errno);
            return 1;
        }
        if (p.pid == 0 && p.passenger_no == -1) { //stop token
            log_msg("KASA", "STOP token t=%d", sim_now());
            break;
        }
        //obsluga pasazera
        if (send_kasa_resp_retry(msg_kasa, p.pid, 1) == -1) { //kasa odsyla pasazerowi odpowiedz przez msg kasa ok=1
            if (g_stop) break; //jesli stop -wyjdz
            log_msg("KASA", "ERROR msgsnd resp errno=%d", errno);
            return 1;
        }
    }

    log_msg("KASA", "KONIEC t=%d", sim_now()); //logujemy koniec 
    return 0;
}