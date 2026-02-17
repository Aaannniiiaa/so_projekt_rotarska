#ifndef COMMON_H
#define COMMON_H
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <stdlib.h>
#include <time.h>


//domyslne parametry symluacji
#define DEF_N 2 //liczba kierowcow
#define DEF_M 50 //liczba pasazerow
#define DEF_INSIDE 20 //ile osob max moze byc w srodku (dzielone na A i B)
#define DEF_P 10 //limit miejsc osobowych w autobusie w kursie
#define DEF_R 2 // limit miejsc rowerowych
#define DEF_T 3 //ile sekund kierowca czeka max na dobor ludzi
#define DEF_Ti 2 //czas powrotu

//losowosc pasazerow
#ifndef VIP_PCT
#define VIP_PCT  0 //szansa na VIP - pomija kase
#endif

#ifndef BIKE_PCT
#define BIKE_PCT 0 //szansa na rower - wchodzi wesjciem B i zasjmuje miejsce rowerowe
#endif

#ifndef SPAWN_MAX_DELAY_MS
#define SPAWN_MAX_DELAY_MS 200 //max losowy delay miedzy spawnami pasazerow
#endif
//^^^

//rozmiary ringow
#define RING_KASA_SIZE   128 
#define RING_BOARD_SIZE  8192

enum {
    SEM_SHM_MUTEX = 0, //ochrona share_state_t //dysp, pas, driver
    SEM_IN_A, //ile miejsc w srodku (osobno piesi i rowery) //pas, driver
    SEM_IN_B, //^^^
    SEM_GATE_A, //bramka do atomowego wejscia
    SEM_GATE_B, //^^
    SEM_KASA_EMPTY, //ring do kasy //ring.c
    SEM_KASA_FULL, //^^
    SEM_KASA_MUTEX, //^^
    SEM_BVIP_EMPTY, //ringi boarding
    SEM_BVIP_FULL, //^^
    SEM_BVIP_MUTEX, //^^
    SEM_BNORM_EMPTY, //^^
    SEM_BNORM_FULL, //^^
    SEM_BNORM_MUTEX, //^^
    SEM_BOARD_ANY, //czy jest cos do zabrania (token dla kierowcow) //pas, driver //rosnie tez przy STOP aby obudzic kierowce gdy konczymy
    SEM_DELAY, //kernel wait //czas powrotu kierowcy
    SEM_SPAWN_DELAY, //^^
    SEM_COUNT_BASE
};

//dane pasazera, to sa inf wrzucane do ringa
//uzywamy w pas buduje req i wrzuca do ring, kasa odbiera ring_kasa i odpowiada do pid, driver wyciaga z boardingu i wysyla inv do pid
//pid=0 pas_no =-1 -> koniec, kasa/driver wychodza
typedef struct {
    pid_t pid;
    int passenger_no;
    int vip;
    int bike;
    int age;
    int is_child;
    int seats;
} passenger_t;

//SHM
//wszystko pod SEM_SHM_MUTEX
typedef struct {
    int stop; //flaga konczenia (dysp ustawia)
    int launch_done; //juz nie bedzie nowych pasazerow
    int served_total; //ile zostalo obsluzonych (driver++)
    int arrived_total; //ile pasazerow dotarlo do boardingu (pas++)
} shared_state_t;

//zeby nie zalac terminala przy duzej ilosci M
//loguje co np 200 pasazer //uzyte w pasazer
#ifndef LOG_EVERY
#define LOG_EVERY 200
#endif

static inline int log_every_hit(int x) {
    return (LOG_EVERY > 0) && (x % LOG_EVERY == 0);
}

//czas symulacji przez env
//zeby kazdy proces mial wspolny czas t=0
#define SIM_ENV_START "SIM_START_EPOCH"

static inline time_t sim_get_start_epoch(void) {
    const char *s = getenv(SIM_ENV_START);
    if (!s || !*s)
        return time(NULL);
    long v = strtol(s, NULL, 10);
    if (v <= 0)
        return time(NULL);
    return (time_t)v;
}

static inline int sim_now(void) {
    time_t start = sim_get_start_epoch();
    time_t now = time(NULL);
    long delta = (long)(now - start);
    if (delta < 0)
        delta = 0;
    if (delta > 2147483647L)
        delta = 2147483647L;
    return (int)delta;
}

#endif