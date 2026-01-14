#pragma once
#include "struct.h"

#define ILO_PAS 5
#define P 5
#define R 1
#define T 1
#define Ti 3
#define N 1

void dyspozytor();
void kierowca(int id_s);
void pasazer(int nr, int id_s, dane *d);
void kasa(int id_s, dane *d);
void unlock(int id_s);
void lock(int id_s);
void signal_driver(int id_s);
void wait_driver(int id_s);
void wait_kasa_odp(int id_s);
void signal_kasa_odp(int id_s);
void signal_kasa_prosba(int id_s);
void wait_kasa_prosba(int id_s);
void signal_kasa_s(int id_s);
void wait_kasa_s(int id_s);