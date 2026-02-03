CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11

ALL=main logger producer consumer dyspozytor station_watch

all: $(ALL)

main: main.o ipc.o synch.o shm.o
	$(CC) $(CFLAGS) -o $@ $^

logger: logger.o
	$(CC) $(CFLAGS) -o $@ $^

producer: producer.o ipc.o synch.o shm.o
	$(CC) $(CFLAGS) -o $@ $^

consumer: consumer.o ipc.o synch.o shm.o
	$(CC) $(CFLAGS) -o $@ $^

dyspozytor: dyspozytor.o shm.o
	$(CC) $(CFLAGS) -o $@ $^

station_watch: station_watch.o shm.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(ALL) raport.txt