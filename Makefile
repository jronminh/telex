CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -Wall -Wextra

all: telex test

telex: telex.c main.c telex.h
	$(CC) $(CFLAGS) -o $@ telex.c main.c

test: telex.c test.c telex.h
	$(CC) $(CFLAGS) -o $@ telex.c test.c

check: test
	./test

stress: telex.c telex.h stress.py
	python3 stress.py

clean:
	rm -f telex test

.PHONY: all check stress clean
