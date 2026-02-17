#define _POSIX_C_SOURCE 200809L
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

static char g_log_path[256] = "sim.log";
static int  g_msgid = -1;

#define LOG_TEXT_MAX 512

typedef struct {
    long mtype;
    char text[LOG_TEXT_MAX];
} log_ipc_msg_t;

void log_set_path(const char* path) {
    if (!path || !*path) return;
    strncpy(g_log_path, path, sizeof(g_log_path) - 1);
    g_log_path[sizeof(g_log_path) - 1] = '\0';
}

void log_reset_file(void) {
    FILE* f = fopen(g_log_path, "w");
    if (f) fclose(f);
}

void log_set_msgid(int msgid) {
    g_msgid = msgid;
}

static void make_ts(char out[32]) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (lt) strftime(out, 32, "%Y-%m-%d %H:%M:%S", lt);
    else snprintf(out, 32, "%ld", (long)now);
}

static int use_color(void){
    const char *e = getenv("LOG_COLOR");
    if (e && *e == '0') return 0;
    return isatty(2);
}

static const char* tag_color(const char* tag){
    if (!tag) return "\033[0m";
    if (strcmp(tag, "DYSPOZYTOR") == 0) return "\033[1;36m";
    if (strcmp(tag, "KIEROWCA") == 0) return "\033[1;33m";
    if (strcmp(tag, "PASAZER") == 0) return "\033[1;32m";
    if (strcmp(tag, "KASA") == 0) return "\033[1;35m";
    if (strcmp(tag, "ERROR") == 0) return "\033[1;31m";
    if (strcmp(tag, "WARN") == 0) return "\033[1;31m";
    return "\033[0m";
}

static void write_stderr(const char* s){
    if (!s) return;
    (void)write(2, s, strlen(s));
}

static void append_file(const char* s){
    FILE* f = fopen(g_log_path, "a");
    if (!f) return;
    fputs(s, f);
    fclose(f);
}

void log_msg(const char* tag, const char* fmt, ...) {
    char msgbuf[LOG_TEXT_MAX];
    char line_plain[LOG_TEXT_MAX];
    char line_col[LOG_TEXT_MAX + 64];
    va_list ap;
    pid_t pid;
    char tag10[11];
    char ts[32];

    if (!tag) tag = "LOG";

    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);

    pid = getpid();
    snprintf(tag10, sizeof(tag10), "%-10.10s", tag);
    make_ts(ts);

    snprintf(line_plain, sizeof(line_plain), "[%s] [pid=%d] %s %.420s\n", ts, (int)pid, tag10, msgbuf);

    if (g_msgid != -1) {
        log_ipc_msg_t m;
        m.mtype = 1;
        strncpy(m.text, line_plain, sizeof(m.text) - 1);
        m.text[sizeof(m.text) - 1] = '\0';
        if (msgsnd(g_msgid, &m, sizeof(m.text), 0) == 0) return;
    }

    if (use_color()) {
        const char *c = tag_color(tag);
        snprintf(line_col, sizeof(line_col), "[%s] [pid=%d] %s%s\033[0m %.420s\n", ts, (int)pid, c, tag10, msgbuf);
        write_stderr(line_col);
    } else {
        write_stderr(line_plain);
    }

    append_file(line_plain);
}
