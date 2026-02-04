CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11

ALL=logger kasa passenger sim_main kasa_test dyspozytor kierowca

all: $(ALL)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

logger: logger.o
	$(CC) $(CFLAGS) -o $@ $^

kasa: kasa.o
	$(CC) $(CFLAGS) -o $@ $^

passenger: passenger.o
	$(CC) $(CFLAGS) -o $@ $^

sim_main: sim_main.o ipc.o kasa_ipc.o shm.o station_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

kasa_test: kasa_test.o ipc.o kasa_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

dyspozytor: dyspozytor.o shm.o station_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

kierowca: kierowca.o shm.o station_ipc.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(ALL) raport.txt