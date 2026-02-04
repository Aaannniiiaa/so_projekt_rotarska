CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11

OBJS_COMMON=shm.o station_ipc.o

all: sim_main dyspozytor kierowca

sim_main: sim_main.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

dyspozytor: dyspozytor.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

kierowca: kierowca.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o sim_main dyspozytor kierowca