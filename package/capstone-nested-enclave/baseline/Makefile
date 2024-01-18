.PHONY: all

all: miniweb cgi/register

cgi/register: cgi/register.c
	$(CC) cgi/register.c -o cgi/register

miniweb: miniweb.o queue.o
	$(CC) miniweb.o queue.o -pthread -o miniweb

miniweb.o: miniweb.c queue.h
	$(CC) miniweb.c -c -g -pthread

queue.o: queue.c queue.h
	$(CC) queue.c -c -g -pthread
