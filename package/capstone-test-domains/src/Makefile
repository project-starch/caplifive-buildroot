.PHONY: all

DOMS_SRC = $(wildcard $(CURDIR)/*.dom.c)
DOMS_S = $(patsubst %.dom.c,%.dom.S,$(DOMS_SRC))
DOMS_TARGETS = $(patsubst %.dom.c,%.dom,$(DOMS_SRC))

all: $(DOMS_TARGETS)


$(DOMS_TARGETS):%:%.S
	$(CC) -static -nostdlib -fPIC -o "$@" "$^"

$(DOMS_S):%.S:%.c
	cd "$(CAPSTONE_CC_PATH)" && cargo run -- --abi capstone "$^" > "$@"
