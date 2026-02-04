#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include "station_ipc.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <shmid> <semid>\n", argv[0]);
        return 2;
    }

    int shmid = atoi(argv[1]);
    int semid = atoi(argv[2]);

    station_state *st = shm_attach(shmid);

    printf("[KIER] pid=%d shmid=%d semid=%d\n", (int)getpid(), shmid, semid);
    fflush(stdout);

    for (;;) {
    st_event_wait(semid);

    st_mutex_P(semid);
    int ev = st->event;
    int koniec = st->koniec;

    // skopiuj stan lokalnie
    int na = st->na_przystanku;

    // czyścimy event od razu
    if (ev != ST_EV_NONE) st->event = ST_EV_NONE;

    // jeśli wymuszenie odjazdu -> jedziemy zawsze
    int force = (ev == ST_EV_ODJAZD);

    // jeśli koniec -> wychodzimy
    if (ev == ST_EV_KONIEC) koniec = 1;

    // warunek normalnego odjazdu (na razie: >= 5)
    // TU potem podmienimy na P z tematu
    int ready = (na >= 5);

    if ((force || ready) && !koniec) {
        st->kurs += 1;
        // na razie: zerujemy ludzi na przystanku, bo "zabraliśmy"
        st->na_przystanku = 0;
    }

    int kurs = st->kurs;
    st_mutex_V(semid);

    if (koniec) {
        printf("[KIER] KONIEC\n");
        fflush(stdout);
        break;
    }

    if (force) {
        printf("[KIER] ODJAZD (force) kurs=%d\n", kurs);
        fflush(stdout);
    } else if (ready) {
        printf("[KIER] ODJAZD (normal) kurs=%d\n", kurs);
        fflush(stdout);
    }
}
    shm_detach(st);
    return 0;
}