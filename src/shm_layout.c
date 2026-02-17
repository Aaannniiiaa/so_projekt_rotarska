#define _POSIX_C_SOURCE 200809L
#include "shm_layout.h"
#include <sys/shm.h>
#include <string.h>

int shm_attach_all(int shmid, void **base_out, shared_state_t **st_out, ring_kasa_t **rk_out, ring_board_t **rb_vip_out, ring_board_t **rb_norm_out)
{
    void *base = shmat(shmid, NULL, 0);
    if (base == (void*)-1) return -1;

    char *ptr = (char*)base;

    shared_state_t* st = (shared_state_t*)ptr;
    ptr += sizeof(shared_state_t);
    ring_kasa_t *rk = (ring_kasa_t*)ptr;
    ptr += sizeof(ring_kasa_t);
    ring_board_t *vip  = (ring_board_t*)ptr;
    ptr += sizeof(ring_board_t);
    ring_board_t *norm = (ring_board_t*)ptr;

    if (base_out) *base_out = base;
    if (st_out) *st_out = st;
    if (rk_out) *rk_out = rk;
    if (rb_vip_out) *rb_vip_out = vip;
    if (rb_norm_out) *rb_norm_out = norm;

    return 0;
}

void shm_detach(void *base)
{
    if (base && base != (void*)-1) (void)shmdt(base);
}