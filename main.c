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
#include "processes.h"
#include "synch.h"

#define ILO_PAS 5
#define POJ 2

int main(){
    key_t key = ftok("main.c", 'A');
    if(key==-1){
        perror("ftok key");
        exit(1);
    }
    printf("Key = %d\n", (int)key);
    int id = shmget(key,sizeof(int)*6, IPC_CREAT | 0600);
    if (id == -1){
        perror("shmget");
        exit(1);
    }
    printf("Id: %d\n", id);

    int *wspolne =shmat(id, NULL, 0);
    int *miejsca=&wspolne[0];
    *miejsca=POJ;
    int *bilety=&wspolne[1];
    *bilety=0;
    int *pozostalo=&wspolne[2];
    *pozostalo=ILO_PAS;
    int *pasazer_nr = &wspolne[3];
    int *pasazer_pid = &wspolne[4];
    int *odpowiedz = &wspolne[5];

    *pasazer_nr=0;
    *pasazer_pid=0;
    *odpowiedz=0;

    key_t key_sem = ftok("main.c", 'B');
    if(key_sem == -1){
        perror("ftok kem sem");
        exit(1);
    }
    
    int id_sem =create_or_get(key_sem);
    int pid_k=fork();
    if(pid_k==0){
        kierowca(id_sem);
    }
    
    int pid_kasa=fork();
    if(pid_kasa==0){
        kasa(id_sem, bilety, miejsca, pasazer_nr, pasazer_pid, odpowiedz);
        exit(0);
    }
    for(int i=1; i<=ILO_PAS; i++){
        int pid = fork();
        if(pid==0){
            pasazer(i, miejsca, id_sem, bilety, pozostalo, pasazer_nr, pasazer_pid, odpowiedz);
        }
    }

    while (wait(NULL)>0){
    }
    printf("Koniec, miejsca=%d\n", *miejsca);
    if(shmdt(wspolne)==-1){
        perror("shmdt");
        exit(1);
    }
    printf("Odlaczono\n");
    if(shmctl(id, IPC_RMID, NULL)==-1){
        perror("shmctl");
        exit(1);
    }
    if(semctl(id_sem, 0, IPC_RMID)==-1){
        perror("semtcl");
        exit(1);
    }
    return 0;
}