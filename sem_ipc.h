#ifndef SEM_IPC_H
#define SEM_IPC_H

#include "common.h"
#include <sys/types.h>
#include <sys/sem.h>
#include <stddef.h>

int  sem_create_clean(int *created);
void sem_remove(int semid);
void sem_lock(int semid);
void sem_unlock(int semid);
int  sem_do(int semid, struct sembuf *ops, size_t nops);
int  sem_setval(int semid, int idx, int val);
int  sem_getval(int semid, int idx);
int  sem_wait_zero(int semid, int idx);
int  sem_add_undo(int semid, int idx, int delta);

#endif