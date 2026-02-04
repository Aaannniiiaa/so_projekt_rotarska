#ifndef STATION_IPC_H
#define STATION_IPC_H

enum {
    SEM_MUTEX = 0,  // mutex do SHM
    SEM_EVENT = 1,  // event "obudź kierowcę"
    SEM_COUNT = 2
};

int  stsem_create(int *created);
void stsem_remove(int semid);

void sem_P(int semid, int idx);
void sem_V(int semid, int idx);

int  sem_getval(int semid, int idx);
void sem_setval(int semid, int idx, int val);

#endif