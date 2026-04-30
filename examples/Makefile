CFLAGS := -fPIC -shared
CC := gcc

all:
	$(CC) -o minimal minimal.c $(CFLAGS)
	$(CC) -o rl rl.c $(CFLAGS) -lraylib
