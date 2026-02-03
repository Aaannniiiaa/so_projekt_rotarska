#ifndef IPC_H
#define IPC_H

#include "common.h"
int  msg_create(void);
void msg_remove(int msgid);

int       shm_create(void);
shm_ring* shm_attach(int shmid);
void      shm_detach(shm_ring* p);
void      shm_remove(int shmid);

int  sem_create(void);
void sem_remove(int semid);

#endif