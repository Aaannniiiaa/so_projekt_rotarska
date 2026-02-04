#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static void install_handlers(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = on_sigint;

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int main(void) {
    install_handlers();
    printf("[KIER] pid=%d start\n", getpid());
    fflush(stdout);

    while (!g_stop) pause();

    printf("[KIER] exit\n");
    fflush(stdout);
    return 0;
}