.PHONY: all

all: $(CURDIR)/sbi.dom

CAPSTONE_C_OUTPUT = $(CURDIR)/sbi.dom.c.S

$(CURDIR)/sbi.dom:$(CURDIR)/sbi.dom.S $(CURDIR)/capstone-sbi/sbi_capstone.S $(CAPSTONE_C_OUTPUT)
	$(CC) -static -nostdlib -fPIC -o "$@" "$<"

$(CAPSTONE_C_OUTPUT):%.c.S:%.c $(CURDIR)/capstone-sbi/sbi_capstone.c
	cd "$(CAPSTONE_CC_PATH)" && cargo run -- --abi capstone "$<" > "$@"
