#include "processes.h"

void dyspozytor(){
    printf("Dyspozytor PID=%d\n", getpid());
    exit(0);
}

void kierowca(int id_s){
    printf("Kierowca czeka\n");
    wait_driver(id_s);
    printf("kierowca odjezdza\n");
    exit(0);
}
void pasazer(int nr, int *miejsca, int id_s, int *bilety, int *pozostalo, int *pasazer_nr, int *pasazer_pid, int *odpowiedz){
    wait_kasa_s(id_s);
    lock(id_s);
    *pasazer_nr=nr;
    *pasazer_pid=getpid();
    *odpowiedz=-1;
    unlock(id_s);

    signal_kasa_prosba(id_s);
    wait_kasa_odp(id_s);

    int kupil=0;
    lock(id_s);
    kupil=*odpowiedz;
    unlock(id_s);
    signal_kasa_s(id_s);

    int ost=0;
    lock(id_s);
    (*pozostalo)--;
    if(*pozostalo==0){
        ost=1;
    }
    unlock(id_s);

    if(!kupil){
        printf("Pasazer %d nie kupil biletu\n", nr);
        if(ost){
            signal_driver(id_s);

        }
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
    if(ost){
        signal_driver(id_s);
    }
    exit(0);
}

void kasa(int id_s, int *bilety, int *miejsca, int *pasazer_nr, int *pasazer_pid, int *odpowiedz){
    for(int i=0; i<ILO_PAS; i++){
        wait_kasa_prosba(id_s);
        lock(id_s);

        if(*bilety < POJ && *miejsca > 0){
            (*bilety)++;
            *odpowiedz=1;
            printf("Sprzedano bilet dla pasazera %d, bilety %d\n", *pasazer_nr, *bilety);
        }
        else{
            *odpowiedz=0;
            printf("Brak biletow dla pasazera %d, bilety %d\n", *pasazer_nr, *bilety);
        }
        unlock(id_s);
        signal_kasa_odp(id_s);
    }
    exit(0);
}