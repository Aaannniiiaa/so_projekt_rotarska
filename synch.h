#pragma once
#include <sys/types.h>
#include <sys/ipc.h>

int create_or_get(key_t key);
void wait_kasa_s(int id_s);
void signal_kasa_s(int id_s);
void wait_kasa_prosba(int id_s);
void signal_kasa_prosba(int id_s);
void signal_kasa_odp(int id_s);
void wait_kasa_odp(int id_s);
void wait_driver(int id_s);
void signal_driver(int id_s);
void lock(int id_s);
void unlock(int id_s);