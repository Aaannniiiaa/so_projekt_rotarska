#ifndef SYNCH_H
#define SYNCH_H

void sem_P(int semid, unsigned short semnum);
void sem_V(int semid, unsigned short semnum);

#endif