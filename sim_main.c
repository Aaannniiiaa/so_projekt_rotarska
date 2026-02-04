#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "station_ipc.h"
#include "common.h"

#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static volatile sig_atomic_t g_stop = 0;
static pid_t g_dysp = -1;
static pid_t g_kier = -1;

static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static void install_sigint(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = on_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) { perror("sigaction(SIGINT)"); exit(1); }
}

static pid_t spawn(const char *prog, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }
    if (pid == 0) {
        execv(prog, argv);
        perror("execv");
        _exit(127);
    }
    return pid;
}

static void wait_one(const char *name, pid_t pid) {
    if (pid <= 0) return;
    int st = 0;
    while (waitpid(pid, &st, 0) == -1 && errno == EINTR) {}
    if (WIFSIGNALED(st)) fprintf(stderr, "%s signaled=%d\n", name, WTERMSIG(st));
    if (WIFEXITED(st) && WEXITSTATUS(st) != 0) fprintf(stderr, "%s exit=%d\n", name, WEXITSTATUS(st));
}

int main(void) {
    install_sigint();

    int created_shm = 0;
    int shmid = shm_create(&created_shm);
    station_state *st = shm_attach(shmid);

    int created_sem = 0;
    int semid = stsem_create(&created_sem);

    if (created_shm) {
        st_mutex_P(semid);
        st->event = ST_EV_NONE;
        st->koniec = 0;
        st->kurs = 0;
        st->na_przystanku = 0;
        st->w_autobusie = 0;
        st_mutex_V(semid);
    }

    char a_shm[32], a_sem[32];
    snprintf(a_shm, sizeof(a_shm), "%d", shmid);
    snprintf(a_sem, sizeof(a_sem), "%d", semid);

    char *argv_d[] = { "./dyspozytor", a_shm, a_sem, NULL };
    char *argv_k[] = { "./kierowca",   a_shm, a_sem, NULL };

    g_dysp = spawn("./dyspozytor", argv_d);
    g_kier = spawn("./kierowca", argv_k);

    printf("PIDS dysp=%d kier=%d\n", (int)g_dysp, (int)g_kier);
    fflush(stdout);

    while (!g_stop) {
        int stw = 0;
        pid_t p = waitpid(-1, &stw, 0);
        if (p == -1) {
            if (errno == EINTR) continue;
            break;
        }
        if (p == g_dysp) g_dysp = -1;
        if (p == g_kier) g_kier = -1;
        if (g_dysp == -1 && g_kier == -1) break;
    }

    if (g_dysp > 0) kill(g_dysp, SIGUSR2);
    if (g_kier > 0) kill(g_kier, SIGTERM);

    wait_one("DYSP", g_dysp);
    wait_one("KIER", g_kier);

    shm_detach(st);
    stsem_remove(semid);
    shm_remove(shmid);

    printf("OK\n");
    return 0;
}