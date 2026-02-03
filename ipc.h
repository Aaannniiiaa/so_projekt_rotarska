#ifndef IPC_H
#define IPC_H

int  msg_create(void);
void msg_remove(int msgid);

int  sem_create(void);
void sem_remove(int semid);

#endif