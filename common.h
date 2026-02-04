#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <stddef.h>

/* ====== FTOK ====== */
#ifndef FTOK_PATH
#define FTOK_PATH "."
#endif

#define FTOK_PROJ_LOG   'L'
#define FTOK_PROJ_KASA  'K'
#define FTOK_PROJ_STSHM 'S'
#define FTOK_PROJ_STSEM 'E'

/* ====== LOG QUEUE ====== */
#define LOG_MTYPE     1L
#define LOG_TEXT_MAX  128

typedef struct {
    long mtype;
    char text[LOG_TEXT_MAX];
} log_msg;

static inline size_t log_msg_size(void) {
    return sizeof(((log_msg*)0)->text);
}

/* ====== KASA QUEUE ====== */
#define KASA_MTYPE_VIP  1L
#define KASA_MTYPE_NORM 2L

typedef struct {
    long  mtype;        /* VIP/NORM */
    pid_t pid;

    int is_vip;
    int has_bike;
    int is_child;
    int with_guardian;
} kasa_req;

static inline size_t kasa_req_size(void) {
    return sizeof(kasa_req) - sizeof(long);
}

typedef struct {
    long mtype;         /* = pid */
    int  ok;
    int  ticket_no;
} kasa_resp;

static inline size_t kasa_resp_size(void) {
    return sizeof(kasa_resp) - sizeof(long);
}

/* ====== STATION / BUS LIMITS (ETAP 3) ====== */
#define ST_MAX_P 50   /* max miejsc (ludzie) */
#define ST_MAX_R 10   /* max rowerów */

/* ====== SHM STATE (wspólny stan przystanku/busa) ====== */
typedef struct {
    int is_open;          /* 1 = trwa boarding, 0 = zamknięte */
    int shutdown;         /* 1 = koniec symulacji */
    int force_departure;  /* 1 = wymuś odjazd */

    int onboard_people;   /* ilu już w autobusie */
    int onboard_bikes;    /* ile rowerów w autobusie */
} station_state;

#endif