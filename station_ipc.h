#ifndef STATION_IPC_H
#define STATION_IPC_H

int stsem_create(int *created);
void stsem_remove(int semid);

void st_mutex_P(int semid);
void st_mutex_V(int semid);

void st_event_wait(int semid);
void st_event_signal(int semid);

#endif