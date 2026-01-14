#pragma once
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/sem.h>
#include <stdbool.h>
#include <errno.h>

#define ILO_PAS 5
#define POJ 2

void dyspozytor();
void kierowca(int id_s);
void pasazer(int nr, int *miejsca, int id_s, int *bilety, int *pozostalo, int *pasazer_nr, int *pasazer_pid, int *odpowiedz);
void kasa(int id_s, int *bilety, int *miejsca, int *pasazer_nr, int *pasazer_pid, int *odpowiedz);
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