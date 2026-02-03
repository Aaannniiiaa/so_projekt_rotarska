#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <sys/types.h>

// ====== FTOK ======
#define FTOK_PATH "."
#define FTOK_PROJ_LOG 'L'
#define FTOK_PROJ_SHM 'M'
#define FTOK_PROJ_SEM 'S'

// ====== LOGGER MSG (kolejka komunikatów) ======
#define LOG_MTYPE    1L
#define LOG_TEXT_MAX 256

typedef struct {
    long mtype;
    char text[LOG_TEXT_MAX];
} log_msg;

static inline size_t log_msg_size(void) {
    return sizeof(((log_msg*)0)->text);
}

// ====== SHM: ring + stan dworca ======
#define RING_SIZE 10

typedef struct {
    int write_idx;
    int read_idx;
    int count;              // debug/podgląd; przy semaforach nie jest konieczne, ale pomaga w watch
    int buf[RING_SIZE];
} shm_ring;

typedef struct {
    int is_open;            // 0/1: czy odjazdy dozwolone
    int shutdown;           // 0/1: zamknięcie na zawsze
    shm_ring ring;
} station_state;

// ====== SEMAFORY dla ring ======
// 0=mutex, 1=empty, 2=full
#define SEM_MUTEX 0
#define SEM_EMPTY 1
#define SEM_FULL  2
#define SEM_COUNT 3

#endif