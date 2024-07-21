CC = gcc
CFLAGS = -std=c11 -pedantic -pthread -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lcurl

all: crawler

crawler: crawler.c
        $(CC) $(CFLAGS) crawler.c -o crawler $(LDFLAGS)

clean:
        rm -f crawler

run: crawler
        ./crawler
