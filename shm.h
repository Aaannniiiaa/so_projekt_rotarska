#ifndef SHM_H
#define SHM_H

#include "common.h"

int shm_create(int *created);
station_state* shm_attach(int shmid);
void shm_detach(station_state *st);
void shm_remove(int shmid);

#endif