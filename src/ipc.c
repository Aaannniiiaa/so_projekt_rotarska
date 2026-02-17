#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/msg.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

//
int sem_setval(int semid, int idx, int val) {
    union semun u; //tworzymy u
    u.val = val; //ustawiamy wartosc
    return semctl(semid, idx, SETVAL, u); //ustawiamy wartosc semafora idx w zestawie semid ; 0-sukces, -1-blad
}

int sem_getval(int semid, int idx) {
    return semctl(semid, idx, GETVAL); //pobieramy aktualna wartosc semafora //uzywamy w driverze jak sprawdzamy czy pusto
}

int sem_down(int semid, int idx) {
    struct sembuf op;
    op.sem_num = (unsigned short)idx; //ktory semafor w zestawie
    op.sem_op = -1; //proces czeka w jadrze az bedzie wieksze od 0
    op.sem_flg = 0; //moze blokowac
    if (semop(semid, &op, 1) == 0) return 0; //wykonaj 1 operacje atomowo
    return -1;
}

int sem_up(int semid, int idx) {
    struct sembuf op;
    op.sem_num = (unsigned short)idx;
    op.sem_op = +1; //zwieksza licznik i budzimy czekajacych
    op.sem_flg = 0;
    if (semop(semid, &op, 1) == 0) return 0;
    return -1;
}

int sem_timeddown_s(int semid, int idx, int timeout_s) {
    struct sembuf op;
    struct timespec ts; //szykujemy strukture operacji i tomeout
    if (timeout_s < 0) timeout_s = 0;
    op.sem_num = (unsigned short)idx;
    op.sem_op = -1;
    op.sem_flg = 0;
    ts.tv_sec = timeout_s;
    ts.tv_nsec = 0;
    return semtimedop(semid, &op, 1, &ts); //timeout w sekundach, jak semafor sie zwolni w czasie - 0, jak minie czas - -1
}

int sem_timeddown_ms(int semid, int idx, int timeout_ms) { //milisekundy
    struct sembuf op;
    struct timespec ts;

    if (timeout_ms < 0) timeout_ms = 0;
    op.sem_num = (unsigned short)idx;
    op.sem_op = -1;
    op.sem_flg = 0;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
    return semtimedop(semid, &op, 1, &ts);
}

int sem_trydown(int semid, int idx) { //nie blokujace
    struct sembuf op;
    struct timespec ts;
    op.sem_num = (unsigned short)idx;
    op.sem_op = -1;
    op.sem_flg = 0;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;

    return semtimedop(semid, &op, 1, &ts);
}

int msg_send_kasa_resp(int msgid, pid_t pid, int ok) { //budujemy wiadomosc
    msg_kasa_resp_t m;
    m.mtype = (long)pid; //kazdy pasazer odbiera swoje odpowiedzi robiac msgrvc
    m.ok = ok;
    return msgsnd(msgid, &m, sizeof(m) - sizeof(long), 0);
}

int msg_recv_kasa_resp(int msgid, pid_t pid, int *ok_out) {
    msg_kasa_resp_t m;
    if (msgrcv(msgid, &m, sizeof(m) - sizeof(long), (long)pid, 0) == -1) //msgrcv blokuje az przyjdzie wiadomosc o typie pid //pas spi
        return -1;
    if (ok_out)
        *ok_out = m.ok;
    return 0;
}

//wiadomosci od kierowcy
int msg_send_invite(int msgid, pid_t pid, const msg_invite_t *inv) {
    msg_invite_t m = *inv;
    m.mtype = (long)pid; //nadpisujemy mtype
    return msgsnd(msgid, &m, sizeof(m) - sizeof(long), 0); //wysylamy
}

int msg_recv_invite(int msgid, pid_t pid, msg_invite_t *inv_out) {
    msg_invite_t m;
    if (msgrcv(msgid, &m, sizeof(m) - sizeof(long), (long)pid, 0) == -1) //pasazer czeka na invite dla siebie //pas spi
        return -1;
    if (inv_out)
        *inv_out = m; //jak dostanie kopiuje struktrue
    return 0;
}