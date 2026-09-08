.PHONY: all

all: $(CURDIR)/sbi.dom

CAPSTONE_C_OUTPUT = $(CURDIR)/sbi.dom.c.S

$(CURDIR)/sbi.dom:$(CURDIR)/sbi.dom.S $(CURDIR)/capstone-sbi/sbi_capstone.S $(wildcard $(CURDIR)/capstone-sbi/*.h) $(CAPSTONE_C_OUTPUT)
	$(CC) -static -nostdlib -fPIC -o "$@" "$<"

# Every source the include chain reaches, not just sbi_capstone.c: a header-only change (the
# per-target geometry in capstone_target.h, 2026-09-08) left sbi.dom.c.S stale and shipped an
# sbi.dom built from the previous constants. Same shape as the wrapper's rule in the top Makefile.
$(CAPSTONE_C_OUTPUT):%.c.S:%.c $(wildcard $(CURDIR)/capstone-sbi/*.c) $(wildcard $(CURDIR)/capstone-sbi/*.h)
	cd "$(CAPSTONE_CC_PATH)" && cargo run -- --abi capstone "$<" > "$@"
