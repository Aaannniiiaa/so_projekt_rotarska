#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "common.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t g_open = 0;
static volatile sig_atomic_t g_shutdown = 0;

static void on_sigusr1(int sig) { (void)sig; g_open = 1; }
static void on_sigusr2(int sig) { (void)sig; g_shutdown = 1; }

static void install_handlers(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = on_sigusr1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) { perror("sigaction(SIGUSR1)"); exit(1); }

    sa.sa_handler = on_sigusr2;
    if (sigaction(SIGUSR2, &sa, NULL) == -1) { perror("sigaction(SIGUSR2)"); exit(1); }
}

int main(void) {
    install_handlers();

    int created = 0;
    int shmid = shm_create(&created);
    station_state *st = shm_attach(shmid, created);

    printf("[DYSP] pid=%d  shmid=%d  created=%d\n", getpid(), shmid, created);
    printf("[DYSP] SIGUSR1 = open departures (st->is_open=1)\n");
    printf("[DYSP] SIGUSR2 = shutdown forever (st->shutdown=1 + exit)\n");
    fflush(stdout);

    while (!g_shutdown) {
        pause(); // czekamy na sygnał (bez sleep, bez mielenia CPU)

        if (g_open) {
            g_open = 0;
            st->is_open = 1;
            printf("[DYSP] OPEN -> st->is_open=1\n");
            fflush(stdout);
        }
    }

    st->shutdown = 1;
    printf("[DYSP] SHUTDOWN -> st->shutdown=1 (exit)\n");
    fflush(stdout);

    shm_detach(st);
    // tu NIE usuwamy shm, bo inne procesy mogą jeszcze chcieć odczytać
    return 0;
}