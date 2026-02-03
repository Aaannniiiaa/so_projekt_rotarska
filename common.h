#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <stddef.h>

// ====== FTOK ======
#define FTOK_PATH "."

// osobne klucze dla różnych zasobów IPC
#define FTOK_PROJ_LOG 'L'
#define FTOK_PROJ_SHM 'M'
#define FTOK_PROJ_SEM 'S'

// ====== LOGGER MSG ======
#define LOG_MTYPE 1L
#define LOG_TEXT_MAX 256

typedef struct {
    long mtype;
    char text[LOG_TEXT_MAX];
} log_msg;

static inline size_t log_msg_size(void) {
    return sizeof(((log_msg*)0)->text);
}

// ====== SHM / SEM (Etap 2+) ======
#define RING_SIZE 10

typedef struct {
    int write_idx;
    int read_idx;
    int buffer[RING_SIZE];
} shm_ring;

// semafory: 0=mutex, 1=empty, 2=full
enum { SEM_MUTEX = 0, SEM_EMPTY = 1, SEM_FULL = 2, SEM_COUNT = 3 };

#endif