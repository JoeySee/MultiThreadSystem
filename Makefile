CC=gcc
CFLAGS=-Wall -std=c18 -g

all: mts

mts: mts.c
	gcc mts.c -pthread -o mts

clean:
	rm -f *.o
	rm -f mts