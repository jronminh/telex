CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -Wall -Wextra

all: telex test

telex: telex.c main.c telex.h
	$(CC) $(CFLAGS) -o $@ telex.c main.c

test: telex.c test.c telex.h
	$(CC) $(CFLAGS) -o $@ telex.c test.c

im-loop: examples/im-loop.c telex.c telex.h
	$(CC) $(CFLAGS) -I. -o examples/im-loop examples/im-loop.c telex.c

check: test
	./test

stress: telex.c telex.h stress.py
	python3 stress.py

clean:
	rm -f telex test examples/im-loop

.PHONY: all check stress im-loop clean
