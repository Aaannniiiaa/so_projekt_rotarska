#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/sem.h>
#include <errno.h>

#define ILO_PAS 5
#define POJ 2

static int create_or_get(key_t key){
    int n_id_sem = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);
    if (n_id_sem!= -1) {
        if (semctl(n_id_sem, 0, SETVAL, 1) == -1) {
            perror("semctl SETVAL");
            exit(1);
        }
        return n_id_sem;
    }
    if (errno == EEXIST) {
        n_id_sem = semget(key, 1, IPC_CREAT | 0600);
        if (n_id_sem == -1) {
            perror("semget existing");
            exit(1);
        }
        return n_id_sem;
    }

    perror("semget");
    exit(1);
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

void dyspozytor(){
    printf("Dyspozytor PID=%d\n", getpid());
    exit(0);
}

void kierowca(){
    printf("Kierowca: PID=%d\n", getpid());
    exit(0);
}

int kup_bilet(int id_s, int *bilety, int *miejsca){
    lock(id_s);
    if(*bilety < POJ && *miejsca >0){
        (*bilety)++;
        printf("Sprzedano bilet, bilety: %d\n", *bilety);
        unlock(id_s);
        return 1;
    }
    else{
        printf("Brak biletow, bilety:%d\n", *bilety);
        unlock(id_s);
        return 0;
    }
}

void pasazer(int nr, int *miejsca, int id_s, int *bilety){
    if(!kup_bilet(id_s, bilety, miejsca)){
        printf("Pasazer nr: %d nie kupil biletu\n", nr);
        exit(0);
    }
    lock(id_s);
    if (*miejsca > 0){
        (*miejsca)--;
        printf("Pasazer %d wsiada, miejsca=%d, PID=%d\n", nr, *miejsca, getpid());
    }
    else
    {
        printf("Pasazer %d, Brak miejsc, PID=%d\n", nr, getpid());
    }
    unlock(id_s);
    exit(0);
}

void kasa(int id_s, int *bilety){
    lock(id_s);
    (*bilety)++;
    printf("Sprzedano bilet, bilety=%d\n", *bilety);
    unlock(id_s);
    exit(0);
}

int main(){
    key_t key = ftok("main.c", 'A');
    if(key==-1){
        perror("ftok key");
        exit(1);
    }
    printf("Key = %d\n", (int)key);
    int id = shmget(key,sizeof(int)*2, IPC_CREAT | 0600);
    if (id == -1){
        perror("shmget");
        exit(1);
    }
    printf("Id: %d\n", id);

    int *wspolne =shmat(id, NULL, 0);
    int *miejsca=&wspolne[0]; //0-miejsca,1-bilety
    *miejsca=POJ;
    int *bilety=&wspolne[1];
    *bilety=0;

    key_t key_sem = ftok("main.c", 'B');
    if(key_sem == -1){
        perror("ftok kem sem");
        exit(1);
    }

    int id_sem =create_or_get(key_sem);
    
    for(int i=1; i<=ILO_PAS; i++){
        int pid = fork();
        if(pid==0){
            pasazer(i, miejsca, id_sem, bilety);
        }
    }

    while (wait(NULL)>0){
    }
    printf("Koniec, miejsca=%d\n", *miejsca);
    if(shmdt(miejsca)==-1){
        perror("shmdt");
        exit(1);
    }
    printf("Odlaczono\n");
    if(shmctl(id, IPC_RMID, NULL)==-1){
        perror("shmctl");
        exit(1);
    }
    return 0;
}