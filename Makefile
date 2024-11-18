CFLAGS = -std=c99 -g -Wall
LFLAGS = -ledit -lm

all: jlisp

jlisp: src/jlisp.c
	gcc -g $(CFLAGS) -o $@ src/*.c $(LFLAGS)

run: jlisp 
	./jlisp
