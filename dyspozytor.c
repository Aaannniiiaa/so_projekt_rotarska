#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static volatile sig_atomic_t g_open_departures = 0;
static volatile sig_atomic_t g_shutdown = 0;

static void on_sigusr1(int sig) {
    (void)sig;
    g_open_departures = 1;
}

static void on_sigusr2(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static void install_handlers(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = on_sigusr1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction(SIGUSR1)");
        exit(1);
    }

    sa.sa_handler = on_sigusr2;
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("sigaction(SIGUSR2)");
        exit(1);
    }
}

int main(void) {
    install_handlers();

    printf("[DYSP] pid=%d\n", getpid());
    printf("[DYSP] SIGUSR1 = open departures\n");
    printf("[DYSP] SIGUSR2 = shutdown (close station forever)\n");
    fflush(stdout);

    while (!g_shutdown) {
        pause(); 

        if (g_open_departures) {
            g_open_departures = 0;
            printf("[DYSP] OPEN: departures allowed now\n");
            fflush(stdout);

        }
    }

    printf("[DYSP] SHUTDOWN: station closed forever\n");
    fflush(stdout);

    return 0;
}