#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <sys/types.h>

// ====== FTOK ======
#define FTOK_PATH "."
#define FTOK_PROJ_LOG 'L'
#define FTOK_PROJ_SHM 'M'
#define FTOK_PROJ_SEM 'S'

// ====== LOGGER MSG ======
#define LOG_MTYPE    1L
#define LOG_TEXT_MAX 256

typedef struct {
    long mtype;
    char text[LOG_TEXT_MAX];
} log_msg;

static inline size_t log_msg_size(void) {
    return sizeof(((log_msg*)0)->text);
}

// ====== SEM ======
#define SEM_MUTEX 0
#define SEM_EMPTY 1
#define SEM_FULL  2
#define SEM_COUNT 3

// ====== SHM: ring + station_state ======
#define RING_SIZE 10

typedef struct {
    int write_idx;
    int read_idx;
    int count;              // 0..RING_SIZE
    int buf[RING_SIZE];     // UWAGA: buf, nie buffer
} shm_ring;

typedef struct {
    int is_open;            // 0/1: odjazdy dozwolone
    int shutdown;           // 0/1: zamknięte na zawsze
    shm_ring ring;          // na przyszłość (kolejka “zdarzeń”)
} station_state;

#endif