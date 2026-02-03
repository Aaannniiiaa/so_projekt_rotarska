#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <msgid>\n", argv[0]);
        return 2;
    }
    int msgid = atoi(argv[1]);

    int fd = open("raport.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) { perror("open raport.txt"); return 1; }

    log_msg m;
    for (;;) {
        // odbieramy tylko LOG_MTYPE
        ssize_t r = msgrcv(msgid, &m, log_msg_size(), LOG_MTYPE, 0);
        if (r == -1) {
            if (errno == EINTR) continue;
            perror("msgrcv logger");
            close(fd);
            return 1;
        }

        // zapisujemy linię + \n
        size_t len = strnlen(m.text, LOG_TEXT_MAX);
        if (write(fd, m.text, len) == -1) { perror("write"); break; }
        if (write(fd, "\n", 1) == -1) { perror("write"); break; }

        if (strcmp(m.text, "STOP") == 0) break;
    }

    close(fd);
    return 0;
}