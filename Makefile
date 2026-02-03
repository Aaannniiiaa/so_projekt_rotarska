CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS =

# Programy
BINS = main logger dyspozytor producer consumer

# Wspólne obiekty
COMMON_OBJS = ipc.o synch.o

all: $(BINS)

# ---------- MAIN ----------
main: main.o ipc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.c common.h ipc.h
	$(CC) $(CFLAGS) -c $<

# ---------- LOGGER ----------
logger: logger.o ipc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

logger.o: logger.c common.h ipc.h
	$(CC) $(CFLAGS) -c $<

# ---------- DYSP ----------
dyspozytor: dyspozytor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dyspozytor.o: dyspozytor.c common.h struct.h
	$(CC) $(CFLAGS) -c $<

# ---------- PRODUCER / CONSUMER (ring test) ----------
producer: producer.o ipc.o synch.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

producer.o: producer.c common.h ipc.h synch.h struct.h
	$(CC) $(CFLAGS) -c $<

consumer: consumer.o ipc.o synch.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

consumer.o: consumer.c common.h ipc.h synch.h struct.h
	$(CC) $(CFLAGS) -c $<

# ---------- IPC / SYNCH ----------
ipc.o: ipc.c ipc.h common.h
	$(CC) $(CFLAGS) -c $<

synch.o: synch.c synch.h common.h struct.h
	$(CC) $(CFLAGS) -c $<

# ---------- UTILS ----------
clean:
	rm -f *.o $(BINS) raport.txt