CFLAGS = -std=c99 -Wall
LFLAGS = -ledit -lm

all: jlisp

jlisp: repl.c
	gcc $(CFLAGS) -o $@ mpc.c repl.c $(LFLAGS)

run: jlisp 
	./jlisp
