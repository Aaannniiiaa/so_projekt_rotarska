#include "processes.h"
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
#include "synch.h"

void dyspozytor(){
    printf("Dyspozytor PID=%d\n", getpid());
    exit(0);
}

void kierowca(int id_s){
    printf("Kierowca czeka\n");
    wait_driver(id_s);
    wait_wejscie(id_s, WEJ_A);
    wait_wejscie(id_s,WEJ_B);
    printf("kierowca odjezdza\n");
    signal_wejscie(id_s, WEJ_A);
    signal_wejscie(id_s, WEJ_B);
    exit(0);
}

void pasazer(int nr, int id_s, dane *d){
    int jest_rower = getpid() %2;
    printf("Pasazer nr %d\n", nr);
    if(jest_rower){
        printf("Pasazer %d ma rower\n", nr);
    }
    else{
        printf("Pasazer %d nie ma roweru\n", nr);
    }
    wait_kasa_s(id_s);
    lock(id_s);
    d->pasazer_nr=nr;
    d->pasazer_pid=getpid();
    d->odpowiedz=-1;

    unlock(id_s);

    signal_kasa_prosba(id_s);
    wait_kasa_odp(id_s);

    int kupil;
    lock(id_s);
    kupil=d->odpowiedz;
    unlock(id_s);
    signal_kasa_s(id_s);

    int ost=0;
    lock(id_s);
    d->pozostalo -=1;
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

    int wejscie;
    if(jest_rower){
        wejscie=WEJ_B;}
        else{
            wejscie=WEJ_A;
        }
    wait_wejscie(id_s, wejscie);
    if(jest_rower){
        printf("Pasazer %d przechodzi przez wejscie B\n", nr);
    }
    else{
        printf("Pasazer %d przechodzi przez wejscie A\n", nr);
    }
    signal_wejscie(id_s, wejscie);

    if(ost){
        signal_driver(id_s);
    }
    exit(0);
}

void kasa(int id_s, dane *d){
    for(int i=0; i<ILO_PAS; i++){
        wait_kasa_prosba(id_s);
        lock(id_s);
        int potrzebne_m=1;
        if(d->dziecko_pas){
            potrzebne_m=2;
        }
        int potrzebny_r=0;
        if(d->rower_pas){
            potrzebny_r=1;
        }
        int m_miejsca=0;
        if(d->miejsca>=potrzebne_m){
            m_miejsca=1;
        }
        int r_miejsca=0;
        if(!potrzebny_r){
            r_miejsca=1;
        }
        else{
            if(d->rowery<R){
                r_miejsca=1;
            }
        }
        int bilet_l=0;
        if(d->bilety < P){
            bilet_l=1;
        }
        if(bilet_l && m_miejsca && r_miejsca){
            d->bilety+=1;
            d->miejsca-=potrzebne_m;
            if(potrzebny_r){
                d->rowery+=1;
            }
            d->odpowiedz=1;
            printf("Sprzedano bilet dla pasazera %d\n", d->pasazer_nr);
        }
        else{
            d->odpowiedz=0;
            if(d->rower_pas && !r_miejsca){
                printf("Pasazer %d ma rower ale brsk miejsca na rower\n", d->pasazer_nr);
            }
            else if(!m_miejsca){
                printf("Brak miejsc dla pasazera %d\n", d->pasazer_nr);
            }
            else if(!bilet_l){
                printf("Brak biletow dla pasazera %d\n", d->pasazer_nr);
            }
            else{
                printf("Pasazer %d nie zostal obsluzony\n", d->pasazer_nr);
            }
        }
        unlock(id_s);
        signal_kasa_odp(id_s);
        
    }
    exit(0);
}