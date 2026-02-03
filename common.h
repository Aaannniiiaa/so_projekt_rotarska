#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define FTOK_PATH "."     // plik/ścieżka do ftok (katalog też działa)
#define FTOK_PROJ 'L'     // "projekt id" do ftok

#define LOG_MTYPE 1L
#define LOG_TEXT_MAX 256

typedef struct {
    long mtype;                 // musi być long
    char text[LOG_TEXT_MAX];    // jedna linia logu
} log_msg;

static inline size_t log_msg_size(void) {
    return sizeof(((log_msg*)0)->text);
}

#endif