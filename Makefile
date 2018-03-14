CC=gcc
CFLAGS=-O2
CFLAGS_BASE=$(CFLAGS) `pkg-config --cflags libmaxminddb`
LDFLAGS=
LDFLAGS_BASE=$(LDFLAGS)  `pkg-config --libs libmaxminddb`
COMP=$(CC) -c $(CFLAGS_BASE) -o
LINK=$(CC) $(LDFLAGS_BASE) -o

.PHONY: all

all: geoiplookuper

main.o : main.c
	$(COMP) $@ $<

geoiplookuper: main.o
	$(LINK) $@ $^
