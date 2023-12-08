.PHONY: all

all: capstone-test

capstone-test: capstone-test.o
	$(CC) -lelf -o $@ $^

capstone-test.o: capstone-test.c
	$(CC) -c -o $@ $^
