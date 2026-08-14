SHELL := /bin/bash
BUILDROOT_EXTERNAL = $(CURDIR)
DEFCONFIG = $(CURDIR)/configs/qemu_capstone_defconfig
CONFIG_PATH = $(CURDIR)/build
CAPSTONE_S_OUTPUT = $(CURDIR)/components/opensbi/lib/sbi/sbi_capstone_dom.c.S \
		$(CURDIR)/components/opensbi/lib/sbi/capstone_int_handler.c.S
CAPSTONE_S_INPUT = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi/sbi_capstone.c
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


# THE MONITOR SOURCE IS A PREREQUISITE, NOT JUST THE ONE-LINE WRAPPER.
#
# sbi_capstone_dom.c is a single `#include "capstone-sbi/sbi_capstone.c"`. With the bare
# %.c.S:%.c rule, make compared the .c.S against that wrapper only, so every edit to the
# monitor itself left the .c.S -- and therefore fw_jump.elf -- silently STALE. The rebuild
# reported success, the firmware kept its byte-identical old size, and the change simply did
# not exist in what QEMU booted. That cost a full misdiagnosis loop today: a monitor fix was
# applied, rebuilt, re-tested, and produced a byte-identical failure at the same pc.
#
# $< is used below rather than $^ because the recipe must pass ONLY the wrapper to the
# compiler; the extra prerequisite is for dependency tracking, not an input file.
#
# package/capstone-sbi-domain/Makefile already declares this dependency correctly, which is
# why that copy never had the problem -- this rule was the odd one out.
$(CAPSTONE_S_OUTPUT):%.c.S:%.c $(CAPSTONE_S_INPUT)
	cd "$(CAPSTONE_CC_PATH)" && if ! /bin/sh -c 'cargo run -- --abi capstone $< -- -I"$(CAPSTONE_S_INCLUDE)" -D__riscv_xlen=64 > "$@"'; then \
		rm -f "$@"; \
		echo "Compilation error. Make sure you supply the correct Capstone-C compiler directory path in CAPSTONE_CC_PATH" >&2; \
		false; \
	fi

clean:
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot clean O="$(CONFIG_PATH)"

