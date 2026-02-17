#ifndef LOG_H
#define LOG_H

#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>

void log_set_path(const char* path);
void log_reset_file(void);
void log_set_msgid(int msgid);
void log_msg(const char* tag, const char* fmt, ...);

#endif