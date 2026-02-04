#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <stddef.h>

#define FTOK_PATH "."

#define FTOK_PROJ_LOG  'L'
#define FTOK_PROJ_KASA 'K'

#define LOG_TEXT_MAX 256
#define LOG_MTYPE 1L

typedef struct {
    long mtype;
    char text[LOG_TEXT_MAX];
} log_msg;

static inline size_t log_msg_size(void) {
    return sizeof(log_msg) - sizeof(long);
}

#define KASA_MTYPE_VIP  1L
#define KASA_MTYPE_NORM 2L

typedef struct {
    long mtype;
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
    long mtype;
    int ok;
    int ticket_no;
} kasa_resp;

static inline size_t kasa_resp_size(void) {
    return sizeof(kasa_resp) - sizeof(long);
}

typedef enum {
    ST_EV_NONE   = 0,
    ST_EV_ODJAZD = 1,
    ST_EV_KONIEC = 2
} station_event;

typedef struct {
    int event;
    int koniec;
    int kurs;
    int na_przystanku;
    int w_autobusie;
} station_state;

#endif