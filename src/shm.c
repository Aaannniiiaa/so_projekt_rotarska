#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "ipc.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static void die(const char* what){
    perror(what);
    exit(1);
}

int st_shm_create_clean(int *created){
    *created = 0;
    key_t k = ipc_key('S');

    for(;;){
        int shmid = shmget(k, sizeof(station_state), 0600 | IPC_CREAT | IPC_EXCL);
        if(shmid != -1){
            *created = 1;
            return shmid;
        }

        if(errno == EEXIST){
            shmid = shmget(k, sizeof(station_state), 0600);
            if(shmid == -1) 
                die("shmget existing");

            if(shmctl(shmid, IPC_RMID, NULL) == -1){
                die("shmctl IPC_RMID (cannot clean old shm)");
            }
            continue;
        }

        die("shmget create");
    }
}

void st_shm_remove(int shmid){
    if(shmid < 0) 
        return;
    if(shmctl(shmid, IPC_RMID, NULL) == -1){
        perror("shmctl IPC_RMID");
    }
}

station_state* shm_attach(int shmid){
    void* p = shmat(shmid, NULL, 0);
    if(p == (void*)-1) 
        die("shmat");
    return (station_state*)p;
}

void shm_detach(station_state* st){
    if(!st) 
        return;
    if(shmdt((void*)st) == -1){
        perror("shmdt");
    }
}

void st_init_if_created(station_state* st, int created, int shmid, int qvip, int qnorm, int log_qid, int semid)
{
    if(!created) 
        return;

    memset(st, 0, sizeof(*st));
    st->event = EV_NONE;
    st->kurs  = 0;
    st->stop  = 0;
    st->force_depart = 0;
    st->shmid = shmid;
    st->kasa_qid_vip  = qvip;
    st->kasa_qid_norm = qnorm;
    st->log_qid = log_qid;
    st->semid   = semid;
    st->next_ticket = 1000;
    st->last_ticket = 999;
    st->P = 1;
    st->R = 0;
    st->onboard = 0;
    st->onboard_bikes = 0;
    st->buses_total = 0;
    st->buses_free = 0;
    st->bus_at_station = 0;
    st->T = 1;
    st->Ti_min = 1;
    st->Ti_max = 1;
}