#ifndef SHM_H
#define SHM_H

#include "common.h"

int            shm_create(int *created);          // tworzy albo otwiera istniejący
station_state* shm_attach(int shmid, int do_init);
void           shm_detach(void *addr);
void           shm_remove(int shmid);

#endif