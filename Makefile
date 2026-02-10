CC = gcc
BASEFLAGS = -Wall -Wextra -O2 -std=c11

# ====== PROFILE (domyślnie fast) ======
MODE ?= fast

# FAST:
# - brak sleep
# - brak printów
# - kierowca rzadko
CFLAGS_fast = $(BASEFLAGS) -DSLEEPY=0 \
  -DPAS_PRINT_EVERY=0 \
  -DKASA_PRINT_EVERY=50 \
  -DKIER_PRINT_EVERY=50

# DEMO:
# - sleep włączony
# - printy "widoczne"
CFLAGS_demo = $(BASEFLAGS) -DSLEEPY=1 \
  -DPAS_PRINT_EVERY=5 \
  -DKASA_PRINT_EVERY=5 \
  -DKIER_PRINT_EVERY=1

# FASTPRINT:
# - brak sleep
# - printy co N
CFLAGS_fastprint = $(BASEFLAGS) -DSLEEPY=0 \
  -DPAS_PRINT_EVERY=500 \
  -DKASA_PRINT_EVERY=500 \
  -DKIER_PRINT_EVERY=20

# TEST5000:
# - brak sleep
# - minimalne logi (żeby szybko dobić do 5000)
CFLAGS_test5000 = $(BASEFLAGS) -DSLEEPY=0 \
  -DPAS_PRINT_EVERY=0 \
  -DKASA_PRINT_EVERY=1000 \
  -DKIER_PRINT_EVERY=100

# wybór profilu:
CFLAGS = $(CFLAGS_$(MODE))

COMMON_DEPS = common.h ipc.h shm.h kasa_ipc.h log_ipc.h sem_ipc.h
ALL = sim_main dyspozytor kierowca kasa passenger_sim logger

all: $(ALL)

# skróty:
fast: MODE=fast
fast: clean all

demo: MODE=demo
demo: clean all

fastprint: MODE=fastprint
fastprint: clean all

test5000: MODE=test5000
test5000: clean all

# build binarek
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

%.o: %.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(ALL) raport.txt

.PHONY: all clean fast demo fastprint test5000