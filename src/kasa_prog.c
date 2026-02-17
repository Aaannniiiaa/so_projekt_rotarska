#define _POSIX_C_SOURCE 200809L
#include "kasa.h"
#include "shm_layout.h"
#include "log.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 4) return 1; //sprawdzamy liczbe argumentow
    int shmid = atoi(argv[1]); //zmiana str na int
    int semid = atoi(argv[2]);
    int msg_kasa = atoi(argv[3]);
    //zmienne do shm attach
    void *base = NULL; //wskaznik na poczatek podpietej pamieci
    shared_state_t *st = NULL; //wsk na shared
    ring_kasa_t *rk = NULL; //wsk na ring kasa
    ring_board_t *v = NULL, *n = NULL; //wsk na ring borad vip i normal

    if (shm_attach_all(shmid, &base, &st, &rk, &v, &n) == -1) return 1; //podpiecie pamieci dzielonej

    int rc = kasa_main(msg_kasa, semid, rk); //uruchamiamy logike kasy, czeka w ring kasa pop, odsyla odp przez msg kasa

    shm_detach(base); //odpiecie
    return rc;
}