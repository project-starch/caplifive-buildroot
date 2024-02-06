.PHONY: all

DOMS_S = $(wildcard $(CURDIR)/*.dom.S)
DOMS_TARGETS = $(patsubst %.dom.S,%.dom,$(DOMS_S))

all: $(DOMS_TARGETS)

$(DOMS_TARGETS):%:%.S
	$(CC) -static -nostdlib -fPIC -o "$@" "$^"
