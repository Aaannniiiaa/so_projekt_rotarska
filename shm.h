#ifndef SHM_H
#define SHM_H

#include "common.h"
#include <sys/types.h>

int st_shm_create_clean(int *created);
void st_shm_remove(int shmid);
station_state* shm_attach(int shmid);
void shm_detach(station_state* st);
void st_init_if_created(station_state* st, int created, int shmid, int qvip, int qnorm, int log_qid, int semid);

#endif