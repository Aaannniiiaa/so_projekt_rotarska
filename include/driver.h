#ifndef DRIVER_H
#define DRIVER_H
#include "common.h"
#include "ring.h"

typedef struct {
    int driver_id;
    int N, M, INSIDE, P, R, T, Ti;
    int msg_invite;
    int semid;
    shared_state_t *st;
    ring_board_t *rb_vip;
    ring_board_t *rb_norm;
} driver_args_t;

int driver_main(driver_args_t a);

#endif