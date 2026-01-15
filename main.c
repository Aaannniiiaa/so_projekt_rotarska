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
#include "struct.h"

int main(){
    key_t key = ftok("main.c", 'A');
    if(key==-1){
        perror("ftok key");
        exit(1);
    }
    printf("Key = %d\n", (int)key);
    int id = shmget(key,sizeof(dane), IPC_CREAT | 0600);
    if (id == -1){
        perror("shmget");
        exit(1);
    }
    printf("Id: %d\n", id);

    dane *d = shmat(id,NULL,0);
    d->miejsca=P;
    d->bilety=0;
    d->pozostalo=ILO_PAS;
    d->pasazer_nr=0;
    d->pasazer_pid=0;
    d->odpowiedz=0;
    d->rowery=0;
    d->rower_pas=0;
    d->dziecko_pas=0;
    d->pas_wiek=0;
    d->do_wejscia=0;
    d->kasa_koniec=0;
    d->kurs=0;
    d->kier_budz=0;
    d->koniec=0;


    key_t key_sem = ftok("main.c", 'B');
    if(key_sem == -1){
        perror("ftok kem sem");
        exit(1);
    }
    
    int id_sem =create_or_get(key_sem);
    int pid_k=fork();
    if(pid_k==0){
        kierowca(id_sem, d);
    }
    
    int pid_kasa=fork();
    if(pid_kasa==0){
        kasa(id_sem, d);
        exit(0);
    }
    for(int i=1; i<=ILO_PAS; i++){
        int pid = fork();
        if(pid==0){
            pasazer(i, id_sem, d);
        }
    }

    while (wait(NULL)>0){
    }
    printf("Koniec, miejsca=%d\n", d->miejsca);
    if(shmdt(d)==-1){
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