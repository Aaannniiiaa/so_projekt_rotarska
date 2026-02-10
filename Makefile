CC = gcc
BASEFLAGS = -Wall -Wextra -O2 -std=c11 -Iinclude

# ====== PROFILE (domyślnie demo) ======
MODE ?= demo

# DEMO:
# - sleep włączony
# - printy "widoczne"
CFLAGS_demo = $(BASEFLAGS) -DSLEEPY=1 \
  -DPAS_PRINT_EVERY=5 \
  -DKASA_PRINT_EVERY=5 \
  -DKIER_PRINT_EVERY=1

# TEST5000:
# - brak sleep
# - minimalne logi (żeby szybko dobić do 5000)
CFLAGS_test5000 = $(BASEFLAGS) -DSLEEPY=0 \
  -DPAS_PRINT_EVERY=0 \
  -DKASA_PRINT_EVERY=1000 \
  -DKIER_PRINT_EVERY=100

CFLAGS = $(CFLAGS_$(MODE))

# nagłówki (w include/)
COMMON_DEPS = include/common.h include/ipc.h include/shm.h include/kasa_ipc.h include/log_ipc.h include/sem_ipc.h

ALL = sim_main dyspozytor kierowca kasa passenger_sim logger

all: $(ALL)

demo: MODE=demo
demo: clean all

test5000: MODE=test5000
test5000: clean all

# ====== linkowanie ======
sim_main: sim_main.o ipc.o shm.o kasa_ipc.o log_ipc.o sem_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

dyspozytor: dyspozytor.o ipc.o shm.o kasa_ipc.o log_ipc.o sem_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

kierowca: kierowca.o ipc.o shm.o sem_ipc.o kasa_ipc.o log_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

kasa: kasa.o ipc.o shm.o kasa_ipc.o log_ipc.o sem_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

passenger_sim: passenger_sim.o ipc.o shm.o kasa_ipc.o sem_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

logger: logger.o ipc.o shm.o log_ipc.o sem_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

# ====== kompilacja obiektów z src/ ======
%.o: src/%.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(ALL) raport.txt

.PHONY: all clean demo test5000