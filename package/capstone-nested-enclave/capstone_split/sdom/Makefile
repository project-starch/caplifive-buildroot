.PHONY: all

DOMS_SMODE_SRC = $(wildcard $(CURDIR)/*.smode.c)
DOMS_SMODE_TARGETS = $(patsubst %.smode.c,%.smode,$(DOMS_SMODE_SRC))

all: $(DOMS_SMODE_TARGETS)

$(DOMS_SMODE_TARGETS):%:%.c
	$(CC) -static -nostdlib -fPIC -fno-stack-protector -o "$@" "$^"
