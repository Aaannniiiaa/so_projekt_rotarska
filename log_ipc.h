#ifndef LOG_IPC_H
#define LOG_IPC_H

#include <sys/types.h>

typedef struct {
    long mtype;
    char text[160];
} log_msg;

int  log_get(int create);
int  log_create_clean(int *created);
void log_remove(int qid);
int  log_send(int qid, const char* text);
int  log_send_stop(int qid);

#endif