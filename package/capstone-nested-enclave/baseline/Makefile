.PHONY: all

all: miniweb cgi/register_success cgi/register_fail

cgi/register_success: cgi/register_success.c
	$(CC) cgi/register_success.c -o cgi/register_success

cgi/register_fail: cgi/register_fail.c
	$(CC) cgi/register_fail.c -o cgi/register_fail

miniweb: miniweb.o queue.o
	$(CC) miniweb.o queue.o -pthread -o miniweb

miniweb.o: miniweb.c queue.h
	$(CC) miniweb.c -c -g -pthread

queue.o: queue.c queue.h
	$(CC) queue.c -c -g -pthread
