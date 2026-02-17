#define _POSIX_C_SOURCE 200809L
#include "pasazer.h"
#include "shm_layout.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 8 ) return 1;

    passenger_args_t a;
    a.no = atoi(argv[1]);
    a.vip = atoi(argv[2]);
    a.bike = atoi(argv[3]);
    int shmid = atoi(argv[4]);
    a.semid = atoi(argv[5]);
    a.msg_kasa = atoi(argv[6]);
    a.msg_invite= atoi(argv[7]);

    void *base = NULL;
    if (shm_attach_all(shmid, &base, &a.st, &a.rk, &a.rb_vip, &a.rb_norm) == -1) return 1;

    int rc = passenger_main(a);

    shm_detach(base);
    return rc;
}