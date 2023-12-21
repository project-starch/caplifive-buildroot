.PHONY: all

TARGETS_C = $(wildcard *.c)
TARGETS_O = $(patsubst %.c,%.o,$(TARGETS_C))
TARGETS = $(patsubst %.c,%.user,$(TARGETS_C))
LIB_O = lib/libcapstone.o
LIB_H = lib/libcapstone.h
LIB_C = lib/libcapstone.c

all: $(TARGETS)

$(LIB_O): $(LIB_C) $(LIB_H)
	$(CC) -c -o $(LIB_O) $(LIB_C)

$(TARGETS_O):%.o:%.c $(LIB_H)
	$(CC) -c -o $@ $<

$(TARGETS):%.user:%.o $(LIB_O)
	$(CC) -o $@ $^
