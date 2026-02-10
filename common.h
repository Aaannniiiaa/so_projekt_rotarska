#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <signal.h>

#ifndef SLEEPY
#define SLEEPY 0
#endif

#if SLEEPY
  #include <time.h>

  static inline void SIM_SLEEP(unsigned sec) {
      struct timespec ts;
      ts.tv_sec  = (time_t)sec;
      ts.tv_nsec = 0;
      nanosleep(&ts, NULL);
  }

  static inline void SIM_USLEEP(unsigned us) {
      struct timespec ts;
      ts.tv_sec  = (time_t)(us / 1000000u);
      ts.tv_nsec = (long)(us % 1000000u) * 1000L;
      nanosleep(&ts, NULL);
  }
#else
  static inline void SIM_SLEEP(unsigned sec) {
    (void)sec;
}
  static inline void SIM_USLEEP(unsigned us) {
    (void)us;
}
#endif

#ifndef PAS_PRINT_EVERY
#define PAS_PRINT_EVERY 0
#endif

#ifndef KASA_PRINT_EVERY
#define KASA_PRINT_EVERY 0
#endif

#ifndef KIER_PRINT_EVERY
#define KIER_PRINT_EVERY 1
#endif

static inline int should_print(long x, int every){
    if(every <= 0) return 0;
    if(every == 1) return 1;
    return (x % every) == 0;
}

typedef enum {
    EV_NONE   = 0,
    EV_ODJAZD = 1,
    EV_KONIEC = 2
} event_t;

enum {
    SEM_MUTEX = 0,
    SEM_SEATS = 1,
    SEM_BIKES = 2,
    SEM_ENTR1 = 3,
    SEM_ENTR2 = 4,

    SEM_QANY  = 5,
    SEM_QVIP  = 6,
    SEM_QNORM = 7,
    SEM_BOARDING = 8,
    SEM_KASA_ANY = 9,
    SEM_BUS_WAKE = 10
};

typedef struct {
    volatile sig_atomic_t event;
    volatile sig_atomic_t kurs;
    volatile sig_atomic_t stop;
    volatile sig_atomic_t force_depart;

    int shmid;
    int kasa_qid_vip;
    int kasa_qid_norm;
    int log_qid;
    int semid;

    pid_t pid_dysp;
    pid_t pid_kier;
    pid_t pid_kasa;
    pid_t pid_logger;

    int next_ticket;
    int cnt_req_total;
    int cnt_vip;
    int cnt_child;
    int cnt_bike;
    int last_ticket;
    int waiting_vip;
    int waiting_norm;
    int P;
    int R;
    int onboard;
    int onboard_bikes;
    int buses_total;
    int buses_free;
    int bus_at_station;
    int T;
    int Ti_min;
    int Ti_max;

} station_state;

#endif