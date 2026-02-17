#define _POSIX_C_SOURCE 200809L
#include "driver.h"
#include "shm_layout.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 12) return 1;

    driver_args_t a;
    a.driver_id = atoi(argv[1]);
    a.N = atoi(argv[2]);
    a.M = atoi(argv[3]);
    a.INSIDE = atoi(argv[4]);
    a.P = atoi(argv[5]);
    a.R = atoi(argv[6]);
    a.T = atoi(argv[7]);
    a.Ti = atoi(argv[8]);
    int shmid = atoi(argv[9]);
    a.semid = atoi(argv[10]);
    a.msg_invite = atoi(argv[11]);

    void *base = NULL;
    ring_kasa_t *rk = NULL;
    if (shm_attach_all(shmid, &base, &a.st, &rk, &a.rb_vip, &a.rb_norm) == -1) return 1;

    int rc = driver_main(a);

    shm_detach(base);
    return rc;
}