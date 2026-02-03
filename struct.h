#ifndef STRUCT_H
#define STRUCT_H

#include "common.h"

typedef struct {
    int depot_open;        // 1=dworzec otwarty, 0=zamkniety
    int stop_all;          // 1=wszystkie procesy maja sie zakonczyc
    int active_workers;    // debug/test: ile workerow jeszcze zyje

    shm_ring q_piesi;
    shm_ring q_rowery;
} shm_state;

#endif