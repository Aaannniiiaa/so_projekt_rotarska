#include "processes.h"
#include "synch.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/sem.h>
#include <stdbool.h>
#include <errno.h>

int create_or_get(key_t key){
    int n_id_sem = semget(key, LICZNIK, IPC_CREAT | IPC_EXCL | 0600);
    if (n_id_sem!= -1) {
        if (semctl(n_id_sem, LOCK, SETVAL, 1) == -1) {
            perror("semctl SETVAL");
            exit(1);
        }
        if(semctl(n_id_sem, DRIVER, SETVAL, 0)==-1){
            perror("semctl driver");
            exit(1);
        }
        
        if (semctl(n_id_sem, PROSBA, SETVAL, 0)==-1){
            perror("semctl kasa_pro");
            exit(1);
        }
        if (semctl(n_id_sem, ODPOWIEDZ, SETVAL, 0)==-1){
            perror("semtcl kasa_odp");
            exit(1);
        }
        if (semctl(n_id_sem, KASA, SETVAL, 1)==-1){
            perror("semctl kasa_s");
            exit(1);
        }
        if(semctl(n_id_sem, WEJ_A, SETVAL, 1)==-1){
            perror("semctl A");
            exit(1);
        }
        if(semctl(n_id_sem, WEJ_B, SETVAL, 1)==-1){
            perror("semctl B");
            exit(1);
        }
        return n_id_sem;
        }
    if (errno == EEXIST) {
        n_id_sem = semget(key, LICZNIK, IPC_CREAT | 0600);
        if (n_id_sem == -1) {
            perror("semget existing");
            exit(1);
        }
        return n_id_sem;
    }

    perror("semget");
    exit(1);
}

void wait_kasa_s(int id_s){
    struct sembuf s = {4,-1,0};
    if(semop(id_s, &s, 1)==-1){
        perror("semop wks");
        exit(1);
    }
}

void signal_kasa_s(int id_s){
    struct sembuf s = {4,1,0};
    if(semop(id_s, &s, 1)==-1){
        perror("semop sks");
        exit(1);
    }
}

void wait_kasa_prosba(int id_s){
    struct sembuf q = {2,-1,0};
    if(semop(id_s,&q, 1)==-1){
        perror("semop q");
        exit(1);
    }
}

void signal_kasa_prosba(int id_s){
    struct sembuf w = {2,1,0};
    if(semop(id_s, &w, 1)==-1){
        perror("semop w");
        exit(1);
    }
}

void signal_kasa_odp(int id_s){
    struct sembuf e={3,1,0};
    if(semop(id_s, &e, 1)==-1){
        perror("semop e");
        exit(1);
    }
}

void wait_kasa_odp(int id_s){
    struct sembuf r = {3,-1,0};
    if(semop(id_s, &r, 1)==-1){
        perror("semop r");
        exit(1);
    }
}

void wait_driver(int id_s){
    struct sembuf y={1,-1,0};
    if(semop(id_s, &y, 1)==-1){
        perror("semop wait driver");
        exit(1);
    }
}

void signal_driver(int id_s){
    struct sembuf z={1,1,0};
    if(semop(id_s, &z, 1)==-1){
        perror("semop signal driver");
        exit(1);
    }
}

void lock(int id_s){
    struct sembuf x = {0, -1, 0};
    if (semop(id_s, &x, 1) == -1){
        perror("semop lock");
        exit(1);
    }
}

void unlock(int id_s){
    struct sembuf x = {0, 1, 0};
    if (semop(id_s, &x, 1)==-1){
        perror("semop unlock");
        exit(1);
    }
}

void wait_wejscie(int id_s, int nr){
    struct sembuf x = {nr, -1, 0};
    if(semop(id_s, &x, 1)==-1){
        perror("semop wait wejscie");
        exit(1);
    }
}

void signal_wejscie(int id_s, int nr){
    struct sembuf y = {nr, 1,0};
    if(semop(id_s, &y, 1)==-1){
        perror("semop signal wejscie");
        exit(1);
    }
}