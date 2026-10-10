# Host-side tests for the portable PixelPoach layers (no GBA headers).
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -I../../src/cam -I../../src
SRC     := ../../src/cam/cam_save.c ../../src/cam/cam_trade.c
OBJS    := $(patsubst %.c,%.o,$(SRC))

.PHONY: all run clean

all: test_save test_trade

test_save: test_save.c cam_save.o cam_trade.o
	$(CC) $(CFLAGS) $^ -o $@

test_trade: test_trade.c cam_save.o cam_trade.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: ../../src/cam/%.c ../../src/cam/%.h
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./test_save
	./test_trade

clean:
	rm -f test_save test_trade *.o
