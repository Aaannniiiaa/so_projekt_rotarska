#ifndef SHM_LAYOUT_H
#define SHM_LAYOUT_H
#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include "ring.h"

int shm_attach_all(int shmid, void **base_out,shared_state_t **st_out, ring_kasa_t **rk_out, ring_board_t **rb_vip_out, ring_board_t **rb_norm_out);
void shm_detach(void *base);

#endif