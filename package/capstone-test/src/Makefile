.PHONY: all

all: capstone-test

capstone-test: capstone-test.o
	$(CC) -o $@ $^

capstone-test.o: capstone-test.c
	$(CC) -c -o $@ $^
