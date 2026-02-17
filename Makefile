CC = gcc

CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic -D_GNU_SOURCE \
  -DLOG_EVERY=1000 \
  -DDRIVER_LOG_EVERY_SERVED=1000 \
  -DDRIVER_LOG_EVERY_COURSE=200 \
  -Iinclude

COMMON_OBJS = src/log.o src/ipc.o src/ring.o src/shm_layout.o

.PHONY: all clean

all: dyspozytor kasa driver pasazer

dyspozytor: src/dyspozytor_main.o src/dyspozytor.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

kasa: src/kasa_prog.o src/kasa.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

driver: src/driver_prog.o src/driver.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

pasazer: src/pasazer_prog.o src/pasazer.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o dyspozytor kasa driver pasazer sim.log