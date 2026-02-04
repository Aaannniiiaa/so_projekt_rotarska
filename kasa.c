#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static void log_send(int logid, const char *txt) {
    log_msg m;
    m.mtype = LOG_MTYPE;
    snprintf(m.text, LOG_TEXT_MAX, "%s", txt);
    for (;;) {
        if (msgsnd(logid, &m, log_msg_size(), 0) == 0) return;
        if (errno == EINTR) continue;
        perror("msgsnd(LOG)");
        _exit(1);
    }
}

static void send_resp(int kasaid, pid_t pid, int ok, int ticket_no) {
    kasa_resp r;
    r.mtype = (long)pid;
    r.ok = ok;
    r.ticket_no = ticket_no;
    for (;;) {
        if (msgsnd(kasaid, &r, kasa_resp_size(), 0) == 0) return;
        if (errno == EINTR) continue;
        perror("msgsnd(KASA_RESP)");
        _exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <kasa_msgid> <log_msgid> <count>\n", argv[0]);
        return 2;
    }

    int kasaid = atoi(argv[1]);
    int logid  = atoi(argv[2]);
    int count  = atoi(argv[3]);

    char line[LOG_TEXT_MAX];
    snprintf(line, sizeof(line), "[KASA pid=%d] start, count=%d", (int)getpid(), count);
    log_send(logid, line);

    int ticket = 1;

    for (int i = 0; i < count; i++) {
        kasa_req req;
        for (;;) {
            // -KASA_MTYPE_NORM => odbierze VIP (1) lub NORM (2), preferując VIP
            ssize_t r = msgrcv(kasaid, &req, kasa_req_size(), -KASA_MTYPE_NORM, 0);
            if (r >= 0) break;
            if (errno == EINTR) continue;
            perror("msgrcv(KASA_REQ)");
            _exit(1);
        }

        int ok = 1;
        int my_ticket = ticket++;

        snprintf(line, sizeof(line),
                 "[KASA] served pid=%d vip=%d child=%d bike=%d => ticket=%d",
                 (int)req.pid, req.is_vip, req.is_child, req.has_bike, my_ticket);
        log_send(logid, line);

        send_resp(kasaid, req.pid, ok, my_ticket);
    }

    log_send(logid, "[KASA] done");
    return 0;
}