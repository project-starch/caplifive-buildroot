.PHONY: all

DOMS_SRC = $(wildcard $(CURDIR)/*.dom.c)
DOMS_S = $(patsubst %.dom.c,%.dom.S,$(DOMS_SRC))
DOMS_TARGETS = $(patsubst %.dom.c,%.dom,$(DOMS_SRC))

DOMS_SMODE_SRC = $(wildcard $(CURDIR)/*.smode.c)
DOMS_SMODE_TARGETS = $(patsubst %.smode.c,%.smode,$(DOMS_SMODE_SRC))

all: $(DOMS_TARGETS) $(DOMS_SMODE_TARGETS)

$(DOMS_SMODE_TARGETS):%:%.c
	$(CC) -static -nostdlib -fPIC -o "$@" "$^"

$(DOMS_TARGETS):%:%.S
	$(CC) -static -nostdlib -fPIC -o "$@" "$^"

$(DOMS_S):%.S:%.c
	cd "$(CAPSTONE_CC_PATH)" && cargo run -- --abi capstone "$^" > "$@"
