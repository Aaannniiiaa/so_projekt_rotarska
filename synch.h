#pragma once
#include <sys/types.h>
#include <sys/ipc.h>

#define LOCK 0
#define DRIVER 1
#define PROSBA 2
#define ODPOWIEDZ 3
#define KASA 4
#define WEJ_A 5
#define WEJ_B 6
#define LICZNIK 7

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
void wait_wejscie(int id_s, int nr);
void signal_wejscie(int id_s, int nr);