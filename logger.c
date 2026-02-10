#define _POSIX_C_SOURCE 200809L
#include "log_ipc.h"
#include "shm.h"
#include "sem_ipc.h"
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "usage: %s <log_qid> <shmid>\n", argv[0]);
        return 1;
    }
    int qid = atoi(argv[1]);
    int shmid = atoi(argv[2]);

    station_state* st = shm_attach(shmid);
    int semid = st->semid;

    sem_lock(semid);
    st->pid_logger = getpid();
    sem_unlock(semid);

    FILE* f = fopen("raport.txt", "w");
    if(!f){
        perror("fopen raport.txt");
        return 1;
    }
    setvbuf(f, NULL, _IONBF, 0);

    for(;;){
        log_msg m;
        ssize_t rc = msgrcv(qid, &m, sizeof(m.text), 0, 0);
        if(rc == -1){
            if(errno == EINTR) 
                continue;
            if(errno == EIDRM) 
                break;
            perror("msgrcv logger");
            break;
        }

        if(m.mtype == 2){
            break;
        }
        if(m.mtype == 1){
            fprintf(f, "%s\n", m.text);
        }
    }

    sem_lock(semid);
    int kurs = (int)st->kurs;
    int total = st->cnt_req_total;
    int vip = st->cnt_vip;
    int child = st->cnt_child;
    int bike = st->cnt_bike;
    int last = st->last_ticket;
    sem_unlock(semid);

    fprintf(f, "\n===== SUMMARY =====\n");
    fprintf(f, "kursy=%d\n", kurs);
    fprintf(f, "pasazerowie_obsluzeni=%d\n", total);
    fprintf(f, "vip=%d\n", vip);
    fprintf(f, "child=%d\n", child);
    fprintf(f, "bike=%d\n", bike);
    fprintf(f, "ostatni_ticket=%d\n", last);
    fprintf(f, "===================\n");

    fclose(f);
    shm_detach(st);

    printf("[LOGGER] stop\n");
    return 0;
}