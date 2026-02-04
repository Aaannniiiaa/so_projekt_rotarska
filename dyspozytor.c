#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "station_ipc.h"
#include "common.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_ev = 0;

static void on_sigusr1(int sig) { (void)sig; g_ev = ST_EV_ODJAZD; }
static void on_sigusr2(int sig) { (void)sig; g_ev = ST_EV_KONIEC; }
static void on_sigint(int sig)  { (void)sig; g_ev = ST_EV_KONIEC; }

static void install_handlers(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = on_sigusr1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) { perror("sigaction SIGUSR1"); exit(1); }

    sa.sa_handler = on_sigusr2;
    if (sigaction(SIGUSR2, &sa, NULL) == -1) { perror("sigaction SIGUSR2"); exit(1); }

    sa.sa_handler = on_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) { perror("sigaction SIGINT"); exit(1); }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <shmid> <semid>\n", argv[0]); return 2; }

    int shmid = atoi(argv[1]);
    int semid = atoi(argv[2]);

    install_handlers();

    station_state *st = shm_attach(shmid);

    printf("[DYSP] pid=%d shmid=%d semid=%d\n", (int)getpid(), shmid, semid);
    fflush(stdout);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1) { perror("sigprocmask"); exit(1); }

    while (!g_stop) {
        while (g_ev == 0) sigsuspend(&oldmask);

        int ev = g_ev;
        g_ev = 0;

        st_mutex_P(semid);
        st->event = ev;
        if (ev == ST_EV_KONIEC) st->koniec = 1;
        st_mutex_V(semid);

        st_event_signal(semid);

        if (ev == ST_EV_ODJAZD) { printf("[DYSP] USR1\n"); fflush(stdout); }
        if (ev == ST_EV_KONIEC) { printf("[DYSP] USR2\n"); fflush(stdout); g_stop = 1; }
    }

    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) { perror("sigprocmask restore"); exit(1); }

    shm_detach(st);
    return 0;
}