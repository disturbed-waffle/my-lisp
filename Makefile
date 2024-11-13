CFLAGS = -std=c99 -Wall
LFLAGS = -ledit -lm

all: jlisp

jlisp: jlisp.c
	gcc -g $(CFLAGS) -o $@ mpc.c jlisp.c $(LFLAGS)

run: jlisp 
	./jlisp
