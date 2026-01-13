#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#define ILO_PAS 5
#define POJ 2

void kasa(){
    printf("Kasa PID=%d\n", getpid());
    exit(0);
}

void dyspozytor(){
    printf("Dyspozytor PID=%d\n", getpid());
    exit(0);
}

void kierowca(){
    printf("Kierowca: PID=%d\n", getpid());
    exit(0);
}

void pasazer(int nr, int *miejsca){
    if (*miejsca > 0){
        (*miejsca)--;
        printf("Pasazer %d wsiada, miejsca=%d, PID=%d\n", nr, *miejsca, getpid());
    }
    else
    {
        printf("Pasazer %d, Brak miejsc, PID=%d\n", nr, getpid());
    }
    exit(0);
}

int main(){
    key_t key = ftok("main.c", 'A');
    printf("Key = %d\n", (int)key);
    int id = shmget(key,sizeof(int), IPC_CREAT | 0600);
    if (id == -1){
        perror("shmget");
        exit(1);
    }
    printf("Id: %d\n", id);

    int *miejsca = shmat(id, NULL, 0);
    *miejsca=POJ;
    printf("Miejsca = %d\n", *miejsca);

    for(int i=1; i<=5; i++){
        int pid = fork();
        if(pid==0){
            pasazer(i, miejsca);
        }
    }

    while (wait(NULL)>0){
    }
    printf("Koniec, miejsca=%d\n", *miejsca);
    if(shmdt(miejsca)==-1){
        perror("shmdt");
        exit(1);
    };
    printf("Odlaczono\n");
    if(shmctl(id, IPC_RMID, NULL)==-1){
        perror("shmctl");
        exit(1);
    };
    return 0;
}