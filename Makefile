CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11

all: main logger

main: main.o ipc.o
	$(CC) $(CFLAGS) -o $@ $^

logger: logger.o
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c common.h ipc.h
	$(CC) $(CFLAGS) -c $<

ipc.o: ipc.c common.h ipc.h
	$(CC) $(CFLAGS) -c $<

logger.o: logger.c common.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o main logger raport.txt