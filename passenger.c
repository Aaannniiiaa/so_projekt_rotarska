#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

static void log_sendf(int logid, const char *fmt, int a, int b, int c, int d, int e) {
    log_msg m;
    m.mtype = LOG_MTYPE;
    snprintf(m.text, LOG_TEXT_MAX, fmt, a, b, c, d, e);
    for (;;) {
        if (msgsnd(logid, &m, log_msg_size(), 0) == 0) return;
        if (errno == EINTR) continue;
        perror("msgsnd(LOG) passenger");
        _exit(1);
    }
}

static int rand_bool_percent(int p) {
    return (rand() % 100) < p;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <kasa_msgid> <log_msgid> [--vip|--norm]\n", argv[0]);
        return 2;
    }

    int kasaid = atoi(argv[1]);
    int logid  = atoi(argv[2]);

    srand((unsigned)(getpid() ^ (unsigned)time(NULL)));

    int vip;
    if (argc >= 4 && strcmp(argv[3], "--vip") == 0) vip = 1;
    else if (argc >= 4 && strcmp(argv[3], "--norm") == 0) vip = 0;
    else vip = rand_bool_percent(1); // domyślnie ~1%

    int bike  = rand_bool_percent(10);
    int child = rand_bool_percent(10);
    int guardian = child ? rand_bool_percent(70) : 0;

    kasa_req req;
    req.mtype = vip ? KASA_MTYPE_VIP : KASA_MTYPE_NORM;
    req.pid = getpid();
    req.is_vip = vip;
    req.has_bike = bike;
    req.is_child = child;
    req.with_guardian = guardian;

    log_sendf(logid, "[PASS pid=%d] to KASA vip=%d bike=%d child=%d guardian=%d",
              (int)req.pid, req.is_vip, req.has_bike, req.is_child, req.with_guardian);

    for (;;) {
        if (msgsnd(kasaid, &req, kasa_req_size(), 0) == 0) break;
        if (errno == EINTR) continue;
        perror("msgsnd(KASA_REQ) passenger");
        _exit(1);
    }

    kasa_resp resp;
    for (;;) {
        ssize_t r = msgrcv(kasaid, &resp, kasa_resp_size(), (long)getpid(), 0);
        if (r >= 0) break;
        if (errno == EINTR) continue;
        perror("msgrcv(KASA_RESP) passenger");
        _exit(1);
    }

    // log: 5 intów, żeby nie robić wariantów snprintf
    log_sendf(logid, "[PASS pid=%d] resp ok=%d ticket=%d child=%d bike=%d",
              (int)getpid(), resp.ok, resp.ticket_no, req.is_child, req.has_bike);

    return resp.ok ? 0 : 1;
}