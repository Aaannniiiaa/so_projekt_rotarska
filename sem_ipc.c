#define _POSIX_C_SOURCE 200809L
#include "sem_ipc.h"
#include "ipc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static void die(const char* what){
    perror(what);
    exit(1);
}

int sem_create_clean(int *created){
    *created = 0;
    key_t k = ipc_key('M');

    for(;;){
        int semid = semget(k, 11, 0600 | IPC_CREAT | IPC_EXCL);
        if(semid != -1){
            *created = 1;
            union semun u;

            u.val = 1;
            if(semctl(semid, SEM_MUTEX, SETVAL, u) == -1)
                die("semctl MUTEX");

            u.val = 0;
            if(semctl(semid, SEM_SEATS, SETVAL, u) == -1)
                die("semctl SEATS");
            if(semctl(semid, SEM_BIKES, SETVAL, u) == -1)
                die("semctl BIKES");
            if(semctl(semid, SEM_ENTR1, SETVAL, u) == -1)
                die("semctl ENTR1");
            if(semctl(semid, SEM_ENTR2, SETVAL, u) == -1)
                die("semctl ENTR2");

            u.val = 1;
            if(semctl(semid, SEM_QANY, SETVAL, u) == -1)
                die("semctl QANY(gate)");

            u.val = 0;
            if(semctl(semid, SEM_QVIP,  SETVAL, u) == -1)
                die("semctl QVIP");
            if(semctl(semid, SEM_QNORM, SETVAL, u) == -1)
                die("semctl QNORM");

            u.val = 0;
            if(semctl(semid, SEM_BOARDING, SETVAL, u) == -1)
                die("semctl BOARDING");

            u.val = 0;
            if(semctl(semid, SEM_KASA_ANY, SETVAL, u) == -1)
                die("semctl KASA_ANY");

            u.val=0;
            if(semctl(semid, SEM_BUS_WAKE, SETVAL, u)==-1)
                die("semctl BUS_WAKE");
            return semid;
        }

        if(errno == EEXIST){
            semid = semget(k, 11, 0600);
            if(semid == -1) 
                die("semget existing");
            if(semctl(semid, 0, IPC_RMID) == -1) 
                die("semctl IPC_RMID(clean old sem)");
            continue;
        }

        die("semget create");
    }
}

void sem_remove(int semid){
    if(semid < 0) 
        return;
    if(semctl(semid, 0, IPC_RMID) == -1){
        perror("semctl IPC_RMID");
    }
}

void sem_lock(int semid){
    if(semid < 0) 
        return;
    struct sembuf op = {SEM_MUTEX, -1, 0};
    for(;;){
        if(semop(semid, &op, 1) == 0) 
            return;
        if(errno == EINTR) 
            continue;
        if(errno == EIDRM || errno == EINVAL) 
            return;
        die("semop lock");
    }
}

void sem_unlock(int semid){
    if(semid < 0) 
        return;
    struct sembuf op = {SEM_MUTEX, +1, 0};
    for(;;){
        if(semop(semid, &op, 1) == 0) 
            return;
        if(errno == EINTR) 
            continue;
        if(errno == EIDRM || errno == EINVAL) 
            return;
        die("semop unlock");
    }
}

int sem_do(int semid, struct sembuf *ops, size_t nops){
    if(semid < 0) 
        return -1;
    for(;;){
        if(semop(semid, ops, (unsigned)nops) == 0) 
            return 0;
        if(errno == EINTR) 
            continue;
        if(errno == EIDRM || errno == EINVAL) 
            return -1;
        return -1;
    }
}

int sem_setval(int semid, int idx, int val){
    union semun u;
    u.val = val;
    return semctl(semid, idx, SETVAL, u);
}

int sem_getval(int semid, int idx){
    return semctl(semid, idx, GETVAL);
}

int sem_wait_zero(int semid, int idx){
    if(semid < 0) 
        return -1;
    struct sembuf op = { (unsigned short)idx, 0, 0 };
    for(;;){
        if(semop(semid, &op, 1) == 0) 
            return 0;
        if(errno == EINTR) 
            continue;
        if(errno == EIDRM || errno == EINVAL) 
            return -1;
        return -1;
    }
}

int sem_add_undo(int semid, int idx, int delta){
    if(semid < 0) 
        return -1;
    struct sembuf op = { (unsigned short)idx, (short)delta, SEM_UNDO };
    for(;;){
        if(semop(semid, &op, 1) == 0) 
            return 0;
        if(errno == EINTR) 
            continue;
        if(errno == EIDRM || errno == EINVAL)
            return -1;
        return -1;
    }
}