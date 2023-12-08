.PHONY: all

all: $(CURDIR)/test.dom


$(CURDIR)/test.dom: $(CURDIR)/test.dom.S
	$(CC) -static -nostdlib -fPIC -o "$@" "$^"

$(CURDIR)/test.dom.S: $(CURDIR)/test.dom.c
	cd "$(CAPSTONE_CC_PATH)" && cargo run -- --abi capstone "$^" > "$@"
