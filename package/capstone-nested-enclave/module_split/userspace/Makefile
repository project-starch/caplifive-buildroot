.PHONY: all

TARGETS_C = miniweb_frontend.c
TARGETS_O = miniweb_frontend.o
TARGETS = miniweb_frontend

all: $(TARGETS)

$(TARGETS_O):%.o:%.c
	$(CC) -c -o $@ $<

$(TARGETS):%:%.o
	$(CC) -o $@ $^
