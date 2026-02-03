#ifndef SHM_H
#define SHM_H

#include "common.h"

// created = 1 jeśli utworzył nowy segment, 0 jeśli już istniał
int shm_create(int *created);

// do_init = 1 → zeruje całą strukturę (tylko gdy created==1)
station_state* shm_attach(int shmid, int do_init);

void shm_detach(void *addr);
void shm_remove(int shmid);

#endif