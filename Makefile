SHELL := /bin/bash
BUILDROOT_EXTERNAL = $(CURDIR)
DEFCONFIG = $(CURDIR)/configs/qemu_capstone_defconfig
CONFIG_PATH = $(CURDIR)/build
CAPSTONE_S_OUTPUT = $(CURDIR)/components/opensbi/lib/sbi/sbi_capstone_dom.c.S $(CURDIR)/components/opensbi/lib/sbi/capstone_int_handler.c.S
CAPSTONE_S_INCLUDE = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi

.PHONY: all setup build clean

all:

setup:
	git submodule update --init --recursive
	mkdir -p overlay
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot defconfig BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" BR2_DEFCONFIG="$(DEFCONFIG)" O="$(CONFIG_PATH)"

build: $(CAPSTONE_S_OUTPUT)
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" $(A)
	if [ -n "$(A)" ]; then \
		LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)"; \
	fi


$(CAPSTONE_S_OUTPUT):%.c.S:%.c
	cd "$(CAPSTONE_CC_PATH)" && if ! /bin/sh -c 'cargo run -- --abi capstone $^ -- -I"$(CAPSTONE_S_INCLUDE)" -D__riscv_xlen=64 > "$@"'; then \
		rm -f "$@"; \
		echo "Compilation error. Make sure you supply the correct Capstone-C compiler directory path in CAPSTONE_CC_PATH" >&2; \
		false; \
	fi

clean:
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot clean O="$(CONFIG_PATH)"

