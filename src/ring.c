#define _POSIX_C_SOURCE 200809L
#include "ring.h"
#include "ipc.h"
#include <errno.h>

//sprzatanie po bledzie
static void sem_down_best(int semid, int idx){
    for(;;){
        if (sem_down(semid, idx) == 0) return; //probujemy sem down
        if (errno == EINTR) continue; //jesli przerwal sygnal to znowu
        return;
    }
}
static void sem_up_best(int semid, int idx){ //^^
    for(;;){
        if (sem_up(semid, idx) == 0) return;
        if (errno == EINTR) continue;
        return;
    }
}
//
int ring_kasa_push(ring_kasa_t *r, int semid, const passenger_t *p) {
    if (sem_down(semid, SEM_KASA_EMPTY) == -1) return -1; //bierzemy wolne miejsca, empty-- //jesli nie ma proces blokuje sie w jadrze
    if (sem_down(semid, SEM_KASA_MUTEX) == -1) { //mutex--
        sem_up_best(semid, SEM_KASA_EMPTY); //jesli sie nie uda oddajemy empty
        return -1;
    }
    r->buf[r->tail] = *p; //zapis do bufora na tail
    r->tail = (r->tail + 1) % RING_KASA_SIZE;

    if (sem_up(semid, SEM_KASA_MUTEX) == -1) { //wyjscie z mutex
        sem_up_best(semid, SEM_KASA_EMPTY);
        return -1;
    }
    if (sem_up(semid, SEM_KASA_FULL) == -1) { //nowy element - full++ //jesli sie nie uda - cofamy 
        sem_down_best(semid, SEM_KASA_MUTEX);
        r->tail = (r->tail - 1 + RING_KASA_SIZE) % RING_KASA_SIZE;
        sem_up_best(semid, SEM_KASA_MUTEX);
        sem_up_best(semid, SEM_KASA_EMPTY);
        return -1;
    }
    return 0;
}

int ring_kasa_pop(ring_kasa_t *r, int semid, passenger_t *out) {
    if (sem_down(semid, SEM_KASA_FULL) == -1) return -1; //czekamy az bedzie element ful--, jak full=0 blokuje
    if (sem_down(semid, SEM_KASA_MUTEX) == -1) { //mutex, jak nie wejdzie oddajemy full bo nie pobralismy elementu
        sem_up_best(semid, SEM_KASA_FULL);
        return -1;
    }

    *out = r->buf[r->head]; //odczyt z head
    r->head = (r->head + 1) % RING_KASA_SIZE;

    if (sem_up(semid, SEM_KASA_MUTEX) == -1) { //wyjscie z mutexa jesli fail znowu oddajemy
        sem_up_best(semid, SEM_KASA_FULL);
        return -1;
    }
    if (sem_up(semid, SEM_KASA_EMPTY) == -1) {
        sem_down_best(semid, SEM_KASA_MUTEX);
        r->head = (r->head - 1 + RING_KASA_SIZE) % RING_KASA_SIZE;
        sem_up_best(semid, SEM_KASA_MUTEX);
        sem_up_best(semid, SEM_KASA_FULL);
        return -1;
    }
    return 0;
}

int ring_kasa_try_push(ring_kasa_t *r, int semid, const passenger_t *p) { //probujemy zabrac empty bez czekania
    if (sem_trydown(semid, SEM_KASA_EMPTY) == -1) { //pas spi
        if (errno == EAGAIN) return 0; 
        return -1;
    }
    if (sem_trydown(semid, SEM_KASA_MUTEX) == -1) {
        sem_up_best(semid, SEM_KASA_EMPTY);
        if (errno == EAGAIN) return 0;
        return -1;
    }

    r->buf[r->tail] = *p;
    r->tail = (r->tail + 1) % RING_KASA_SIZE;

    sem_up_best(semid, SEM_KASA_MUTEX);

    if (sem_up(semid, SEM_KASA_FULL) == -1) {
        sem_down_best(semid, SEM_KASA_MUTEX);
        r->tail = (r->tail - 1 + RING_KASA_SIZE) % RING_KASA_SIZE;
        sem_up_best(semid, SEM_KASA_MUTEX);
        sem_up_best(semid, SEM_KASA_EMPTY);
        return -1;
    }
    return 1;
}


int ring_board_push(ring_board_t *r, int semid, const passenger_t *p, int sem_empty, int sem_full, int sem_mutex)
{
    if (sem_down(semid, sem_empty) == -1) return -1;
    if (sem_down(semid, sem_mutex) == -1) {
        sem_up_best(semid, sem_empty);
        return -1;
    }

    r->buf[r->tail] = *p;
    r->tail = (r->tail + 1) % RING_BOARD_SIZE;

    if (sem_up(semid, sem_mutex) == -1) {
        sem_up_best(semid, sem_empty);
        return -1;
    }
    if (sem_up(semid, sem_full) == -1) {
        sem_down_best(semid, sem_mutex);
        r->tail = (r->tail - 1 + RING_BOARD_SIZE) % RING_BOARD_SIZE;
        sem_up_best(semid, sem_mutex);
        sem_up_best(semid, sem_empty);
        return -1;
    }
    return 0;
}

int ring_board_pop(ring_board_t *r, int semid, passenger_t *out, int sem_empty, int sem_full, int sem_mutex)
{
    if (sem_down(semid, sem_full) == -1) return -1;
    if (sem_down(semid, sem_mutex) == -1) {
        sem_up_best(semid, sem_full);
        return -1;
    }

    *out = r->buf[r->head];
    r->head = (r->head + 1) % RING_BOARD_SIZE;

    if (sem_up(semid, sem_mutex) == -1) {
        sem_up_best(semid, sem_full);
        return -1;
    }
    if (sem_up(semid, sem_empty) == -1) {
        sem_down_best(semid, sem_mutex);
        r->head = (r->head - 1 + RING_BOARD_SIZE) % RING_BOARD_SIZE;
        sem_up_best(semid, sem_mutex);
        sem_up_best(semid, sem_full);
        return -1;
    }
    return 0;
}

int ring_board_try_pop(ring_board_t *r, int semid, passenger_t *out, int sem_empty, int sem_full, int sem_mutex)
{
    if (sem_trydown(semid, sem_full) == -1) {
        if (errno == EAGAIN) return 0;
        return -1;
    }
    if (sem_trydown(semid, sem_mutex) == -1) {
        sem_up_best(semid, sem_full);
        if (errno == EAGAIN) return 0;
        return -1;
    }

    *out = r->buf[r->head];
    r->head = (r->head + 1) % RING_BOARD_SIZE;

    sem_up_best(semid, sem_mutex);

    if (sem_up(semid, sem_empty) == -1) {
        sem_down_best(semid, sem_mutex);
        r->head = (r->head - 1 + RING_BOARD_SIZE) % RING_BOARD_SIZE;
        sem_up_best(semid, sem_mutex);
        sem_up_best(semid, sem_full);
        return -1;
    }
    return 1;
}

int ring_board_try_push(ring_board_t *r, int semid, const passenger_t *p, int sem_empty, int sem_full, int sem_mutex)
{
    if (sem_trydown(semid, sem_empty) == -1) {
        if (errno == EAGAIN) return 0;
        return -1;
    }
    if (sem_trydown(semid, sem_mutex) == -1) {
        sem_up_best(semid, sem_empty);
        if (errno == EAGAIN) return 0;
        return -1;
    }

    r->buf[r->tail] = *p;
    r->tail = (r->tail + 1) % RING_BOARD_SIZE;

    sem_up_best(semid, sem_mutex);

    if (sem_up(semid, sem_full) == -1) {
        sem_down_best(semid, sem_mutex);
        r->tail = (r->tail - 1 + RING_BOARD_SIZE) % RING_BOARD_SIZE;
        sem_up_best(semid, sem_mutex);
        sem_up_best(semid, sem_empty);
        return -1;
    }
    return 1;
}