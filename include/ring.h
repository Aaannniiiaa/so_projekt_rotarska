#ifndef RING_H
#define RING_H
#define _POSIX_C_SOURCE 200809L
#include "common.h"

//SHM - kolejka do kasy
typedef struct {
    int head; //skad czytamy
    int tail; //gdzie zapisujemy
    passenger_t buf[RING_KASA_SIZE]; //tablica
} ring_kasa_t;

//SHM - boarding
typedef struct {
    int head;
    int tail;
    passenger_t buf[RING_BOARD_SIZE];
} ring_board_t;

int ring_kasa_push(ring_kasa_t *r, int semid, const passenger_t *p);
int ring_kasa_pop (ring_kasa_t *r, int semid, passenger_t *out);
int ring_kasa_try_push(ring_kasa_t *r, int semid, const passenger_t *p);
int ring_board_push(ring_board_t *r, int semid, const passenger_t *p, int sem_empty, int sem_full, int sem_mutex);
int ring_board_pop(ring_board_t *r, int semid, passenger_t *out, int sem_empty, int sem_full, int sem_mutex);
int ring_board_try_pop(ring_board_t *r, int semid, passenger_t *out, int sem_empty, int sem_full, int sem_mutex);
int ring_board_try_push(ring_board_t *r, int semid, const passenger_t *p, int sem_empty, int sem_full, int sem_mutex);

#endif