#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include "kasa_ipc.h"
#include "common.h"
#include "shm.h"
#include "shm.h"
#include "station_ipc.h"
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// ===================== GLOBALS (do sprzątania) =====================
static volatile sig_atomic_t g_stop = 0;

static int  g_logid  = -1;
static int  g_kasaid = -1;

static pid_t g_pid_logger = -1;
static pid_t g_pid_kasa   = -1;
static pid_t g_pid_dysp   = -1;
static pid_t g_pid_kier   = -1;

// ===================== SIGNALS =====================
static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

// ===================== HELPERS =====================
static void log_send(int msgid, const char *txt) {
    if (msgid < 0) return;

    log_msg m;
    m.mtype = LOG_MTYPE;
    snprintf(m.text, LOG_TEXT_MAX, "%s", txt);

    for (;;) {
        if (msgsnd(msgid, &m, log_msg_size(), 0) == 0) return;
        if (errno == EINTR) continue;
        // jeśli kolejka już usunięta w cleanup, nie rób paniki
        return;
    }
}

static pid_t spawnv(const char *prog, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }
    if (pid == 0) {
        // dzieci nie przechwytują SIGINT z terminala (steruje sim_main)
        signal(SIGINT, SIG_IGN);
        execv(prog, argv);
        perror("execv");
        _exit(127);
    }
    return pid;
}

static void kill_if_running(pid_t pid, int sig) {
    if (pid > 0) kill(pid, sig);
}

static void wait_one(const char *name, pid_t pid) {
    if (pid <= 0) return;

    int st = 0;
    for (;;) {
        if (waitpid(pid, &st, 0) >= 0) break;
        if (errno == EINTR) continue;
        perror("waitpid");
        return;
    }

    if (WIFEXITED(st) && WEXITSTATUS(st) != 0) {
        fprintf(stderr, "%s pid=%d exit=%d\n", name, (int)pid, WEXITSTATUS(st));
    } else if (WIFSIGNALED(st)) {
        fprintf(stderr, "%s pid=%d signaled=%d\n", name, (int)pid, WTERMSIG(st));
    }
}

// sprzątanie uruchamiane i przy normalnym końcu i przy Ctrl+C
static void cleanup(void) {
    // 1) poproś procesy pomocnicze o wyjście
    kill_if_running(g_pid_dysp, SIGTERM);
    kill_if_running(g_pid_kier, SIGTERM);

    // 2) jeśli kasa jeszcze żyje — zakończ (SIGTERM)
    kill_if_running(g_pid_kasa, SIGTERM);

    // 3) logger dostaje STOP (żeby domknął raport)
    log_send(g_logid, "STOP");

    // 4) na wszelki wypadek, jakby logger dalej wisiał
    kill_if_running(g_pid_logger, SIGTERM);

    // 5) poczekaj na dzieci (żeby nie robić zombie)
    wait_one("DYSP",   g_pid_dysp);
    wait_one("KIER",   g_pid_kier);
    wait_one("KASA",   g_pid_kasa);
    wait_one("LOGGER", g_pid_logger);

    // 6) usuń kolejki IPC (żeby nie zostawiać śmieci)
    if (g_kasaid != -1) kasaq_remove(g_kasaid);
    if (g_logid  != -1) msg_remove(g_logid);

    g_kasaid = -1;
    g_logid  = -1;
}

// ===================== MAIN =====================
int main(int argc, char **argv) {
    // sygnały
int c_shm = 0;
int shmid = shm_create(&c_shm);
station_state *st = shm_attach(shmid);

int c_sem = 0;
int semid = stsem_create(&c_sem);

// init minimalny (tylko jeśli stworzone nowe)
if (c_sem) {
    sem_setval(semid, SEM_MUTEX, 1);
    sem_setval(semid, SEM_EVENT, 0);
}

// test P/V
sem_P(semid, SEM_MUTEX);
sem_V(semid, SEM_MUTEX);

shm_detach(st);
shm_remove(shmid);
stsem_remove(semid);

printf("SHM+SEM TEST OK (shmid=%d semid=%d)\n", shmid, semid);
return 0;

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = on_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
        return 1;
    }

    // sprzątanie zawsze (także przy Ctrl+C, jeśli wyjdziemy przez return/exit)
    atexit(cleanup);

    int N = 30;
    if (argc >= 2) N = atoi(argv[1]);
    if (N <= 0) N = 30;

    // kolejki
    g_logid  = msg_create_clean();
    g_kasaid = kasaq_create_clean();

    char logbuf[32], kasabuf[32], nbuf[32];
    snprintf(logbuf,  sizeof(logbuf),  "%d", g_logid);
    snprintf(kasabuf, sizeof(kasabuf), "%d", g_kasaid);
    snprintf(nbuf,    sizeof(nbuf),    "%d", N);

    // logger
    char *argv_logger[] = { "./logger", logbuf, NULL };
    g_pid_logger = spawnv("./logger", argv_logger);

    log_send(g_logid, "START SIM_MAIN");

    // kasa
    char *argv_kasa[] = { "./kasa", kasabuf, logbuf, nbuf, NULL };
    g_pid_kasa = spawnv("./kasa", argv_kasa);

    // et3: nowe procesy (szkielet)
    char *argv_dysp[] = { "./dyspozytor", NULL };
    g_pid_dysp = spawnv("./dyspozytor", argv_dysp);

    char *argv_kier[] = { "./kierowca", NULL };
    g_pid_kier = spawnv("./kierowca", argv_kier);

    // pasażerowie
    pid_t *pass = calloc((size_t)N, sizeof(pid_t));
    if (!pass) { perror("calloc"); return 1; }

    int spawned = 0;
    for (int i = 0; i < N; i++) {
        if (g_stop) break;
        char *argv_p[] = { "./passenger", kasabuf, logbuf, NULL };
        pass[i] = spawnv("./passenger", argv_p);
        spawned++;
    }

    // poczekaj na tych, których uruchomiłaś
    for (int i = 0; i < spawned; i++) {
        wait_one("PASS", pass[i]);
    }
    free(pass);

    // jeśli przerwane Ctrl+C, to nie czekamy w nieskończoność na kasę
    if (!g_stop) {
        wait_one("KASA", g_pid_kasa);
        g_pid_kasa = -1; // już zebrany
    }

    // grzeczne zamknięcie dysp/kier (żeby nie wisiały)
    kill_if_running(g_pid_dysp, SIGTERM);
    kill_if_running(g_pid_kier, SIGTERM);

    wait_one("DYSP", g_pid_dysp); g_pid_dysp = -1;
    wait_one("KIER", g_pid_kier); g_pid_kier = -1;

    // domknij log i poczekaj na logger
    log_send(g_logid, "STOP");
    wait_one("LOGGER", g_pid_logger);
    g_pid_logger = -1;

    // usuń kolejki zanim wyjdziemy (żeby ipcs -q było czyste)
    kasaq_remove(g_kasaid); g_kasaid = -1;
    msg_remove(g_logid);    g_logid  = -1;

    printf("OK. See raport.txt\n");
    return 0;
}