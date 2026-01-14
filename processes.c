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
void pasazer(int nr, int id_s, dane *d){
    wait_kasa_s(id_s);
    lock(id_s);
    d->pasazer_nr=nr;
    d->pasazer_pid=getpid();
    d->odpowiedz=-1;
    unlock(id_s);

    signal_kasa_prosba(id_s);
    wait_kasa_odp(id_s);

    int kupil=0;
    lock(id_s);
    kupil=d->odpowiedz;
    unlock(id_s);
    signal_kasa_s(id_s);

    int ost=0;
    lock(id_s);
    (d->pozostalo)--;
    if(d->pozostalo==0){
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

    if (d->miejsca > 0){
        (d->miejsca)--;
        printf("Pasazer %d wsiada, miejsca=%d, PID=%d\n", nr, d->miejsca, getpid());
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

void kasa(int id_s, dane *d){
    for(int i=0; i<ILO_PAS; i++){
        wait_kasa_prosba(id_s);
        lock(id_s);

        if(d->bilety < P && d->miejsca > 0){
            (d->bilety)++;
            d->odpowiedz=1;
            printf("Sprzedano bilet dla pasazera %d, bilety %d\n", d->pasazer_nr, d->bilety);
        }
        else{
            d->odpowiedz=0;
            printf("Brak biletow dla pasazera %d, bilety %d\n", d->pasazer_nr, d->bilety);
        }
        unlock(id_s);
        signal_kasa_odp(id_s);
    }
    exit(0);
}