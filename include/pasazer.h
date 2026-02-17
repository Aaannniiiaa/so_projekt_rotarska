#ifndef PASAZER_H
#define PASAZER_H
#include "common.h"
#include "ring.h"

typedef struct {
    int no, vip, bike;
    int msg_kasa, msg_invite;
    int semid;
    shared_state_t *st;
    ring_kasa_t  *rk;
    ring_board_t *rb_vip;
    ring_board_t *rb_norm;
} passenger_args_t;

int passenger_main(passenger_args_t a);

#endif