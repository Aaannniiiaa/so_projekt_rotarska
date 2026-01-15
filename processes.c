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
#include "struct.h"

void dyspozytor(){
    printf("Dyspozytor PID=%d\n", getpid());
    exit(0);
}

void kierowca(int id_s, dane *d){
    for(int i=0; i<N; i++){
        lock(id_s);
        d->kurs++;
        d->kier_budz = 0;
        d->miejsca = P;
        d->bilety = 0;
        d->rowery = 0;
        d->do_wejscia = 0;
        d->kasa_koniec = 0;
        unlock(id_s);

        printf("[KIEROWCA] kurs %d: ladowanie %d s\n", d->kurs, T);

        sleep(T);
        lock(id_s);
        d->kasa_koniec = 1;
        int juz_pusto = (d->do_wejscia == 0);
        if(juz_pusto){
            d->kier_budz = 1;
        }
        unlock(id_s);
        if(!juz_pusto){
            wait_driver(id_s);
        }
        wait_wejscie(id_s, WEJ_A);
        wait_wejscie(id_s, WEJ_B);

        printf("[KIEROWCA] kurs %d: odjezdza\n", d->kurs);
        sleep(Ti);

        printf("[KIEROWCA] kurs %d: wraca\n", d->kurs);

        signal_wejscie(id_s, WEJ_A);
        signal_wejscie(id_s, WEJ_B);

        lock(id_s);
        if(d->pozostalo <= 0){
            d->koniec = 1;
            unlock(id_s);
            break;
        }
        unlock(id_s);
    
    }
    lock(id_s);
    d->koniec = 1;
    unlock(id_s);
    signal_kasa_prosba(id_s);
    exit(0);
}

void pasazer(int nr, int id_s, dane *d){
    srand(getpid());
    int wiek=rand()%15;
    int dziecko=(wiek < 8);
    
    int ost_kurs=-1;
    if(wiek<8){
        dziecko=1;
    }
    int rower=0;
    if(rand()%2==1){
        rower=1;
    }
    printf("Pasazer nr %d, wiek=%d\n", nr, wiek);
    if(dziecko){
        printf("Pasazer %d to dziecko\n", nr);
    }
    if(rower){
        printf("Pasazer %d ma rower\n", nr);
    }
    else{
        printf("Pasazer %d nie ma roweru\n", nr);
    }
    while(1){
    wait_kasa_s(id_s);
    lock(id_s);
    d->pasazer_nr=nr;
    d->pasazer_pid=getpid();
    d->odpowiedz=-1;
    d->pas_wiek=wiek;
    d->dziecko_pas=dziecko;
    d->rower_pas=rower;
    unlock(id_s);

    signal_kasa_prosba(id_s);
    wait_kasa_odp(id_s);
    lock(id_s);
    int kupil;
    kupil=d->odpowiedz;
    int k= d->kurs;
    int koniec=d->koniec;
    unlock(id_s);
    signal_kasa_s(id_s);

    if(koniec){
    exit(0);}

    if(!kupil){
        printf("Pasazer %d nie kupil biletu\n", nr);
        
        lock(id_s);
        int budz= (d->kasa_koniec==1 && d->do_wejscia==0 && d->kier_budz==0);
        if(budz){
            d->kier_budz=1;
        }
        
        unlock(id_s);
         if(budz){
        signal_driver(id_s);}
        
        while(1){
            lock(id_s);
            int nowy=(d->kurs != k) || d->koniec;
            unlock(id_s);
            if(nowy){
                break;
            }
            usleep(100000);
        }
        lock(id_s);
        if(d->koniec){
            unlock(id_s);
            exit(0);
        }
        unlock(id_s);
        continue;
    }
        int wejscie = rower ? WEJ_B : WEJ_A;
        wait_wejscie(id_s, wejscie);
        if(dziecko){
        if(wejscie== WEJ_A){
            printf("Opiekun pasazera %d przechodzi przez A\n", nr);
            printf("Dziecko pasazera %d przechodzi przez A\n", nr);
            lock(id_s);
            d->do_wejscia-=2;
            unlock(id_s);

        }
        else{
            printf("Opiekun pasazera %d przechodzi przez B\n", nr);
            printf("Dziecko pasazera %d przechodzi przez B\n", nr);
        lock(id_s);
        d->do_wejscia -=2;
        unlock(id_s);
        }
    }else{
        if (wejscie == WEJ_A){
            printf("[PASAZER] Pasazer %d przechodzi przez A\n", nr);
            lock(id_s);
        d->do_wejscia -=1;
        unlock(id_s);
        }
        else{
            printf("[PASAZER] Pasazer %d przechodzi przez B\n", nr);
             lock(id_s);
        d->do_wejscia -=1;
        unlock(id_s);
        }
    }
    signal_wejscie(id_s, wejscie);
    lock(id_s);
    int budz2 = (d->kasa_koniec ==1 && d->do_wejscia==0 && d->kier_budz==0);
    if(budz2) {
    d->kier_budz=1;}
    unlock(id_s);
    if(budz2){
    signal_driver(id_s);}
    lock(id_s);
    d->pozostalo--;
    unlock(id_s);
    exit(0);
}}

void kasa(int id_s, dane *d){
    while(1){
        wait_kasa_prosba(id_s);
        lock(id_s);
        if(d->koniec){
            d->odpowiedz=0;
            unlock(id_s);
            signal_kasa_odp(id_s);
            exit(0);
        }
        if(d->kasa_koniec){
            d->odpowiedz=0;
            unlock(id_s);
            signal_kasa_odp(id_s);
            continue;
        }
    
        int potrzebne_m=1;
        if(d->dziecko_pas){
            potrzebne_m=2;
        }
        int potrzebne_r=0;
        if(d->rower_pas){
            potrzebne_r=1;
        }
        int m_miejsca = (d->miejsca>=potrzebne_m);
        int r_miejsca = (!potrzebne_r || d->rowery < R);
        int bilet_l = (d->bilety < P);

        if (m_miejsca && r_miejsca && bilet_l){
            d->bilety+=1;
            d->miejsca-=potrzebne_m;
            if(potrzebne_r){
                d->rowery+=1;
            }
            d->do_wejscia += potrzebne_m;
            d->odpowiedz=1;
            printf("[KASA] Sprzedano bilet dla pasazera %d\n", d->pasazer_nr);
            if(d->bilety>=P || d->miejsca <= 0){
                d->kasa_koniec = 1;
            }
        }
        else{
            d->odpowiedz=0;
            printf("[KASA] Brak miejsc dla pasazera %d\n", d->pasazer_nr);
            if(d->bilety>=P || d->miejsca <=0){
                d->kasa_koniec=1;
            }
        }
        int budz=0;
        if(d->kasa_koniec && d->do_wejscia==0 && d->kier_budz==0){
            d->kier_budz=1;
            budz=1;
            /*(id_s);
            signal_kasa_odp(id_s);
            signal_driver(id_s);
            continue;*/
        }

        unlock(id_s);
        signal_kasa_odp(id_s);
     
        if(budz){
            signal_driver(id_s);
        }
    }
  
}
