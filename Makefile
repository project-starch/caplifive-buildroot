SHELL := /bin/bash

# ONE TREE, TWO TARGETS.
#
# TARGET=fpga (default) builds the board firmware: fpga_defconfig, OpenSBI platform fpga/ariane,
# the kernel and initramfs embedded into fw_payload.bin. TARGET=qemu builds the QEMU stand-in:
# qemu_capstone_defconfig, platform generic, fw_jump.elf plus an ext2 rootfs. Each target owns its
# OWN output directory, build-$(TARGET). That separation is not tidiness: with one shared build/
# the FPGA firmware's fw_jump.o -- compiled with an embedded device tree -- was relinked into the
# QEMU firmware by `A=opensbi-rebuild`, and QEMU then discarded the DTB it was handed and hung with
# zero serial output before any banner (ISSUES.md C-11). Two directories make that impossible.
#
# The harness scripts keep reading `build/`: make it a per-checkout symlink to the directory of the
# target that checkout serves (build -> build-fpga in the board checkout, build -> build-qemu in the
# QEMU one). It is untracked on purpose -- the symlink is a property of the checkout, not of the tree.
TARGET ?= fpga
ifeq ($(filter $(TARGET),fpga qemu),)
$(error TARGET must be 'fpga' or 'qemu' (got '$(TARGET)'))
endif

# Never -j $(nproc): on the shared build host a full-width parallel link storm has taken the whole
# machine down. 90 of 112 is the standing rule.
JOBS ?= 90

BUILDROOT_EXTERNAL = $(CURDIR)
CONFIG_PATH := $(CURDIR)/build-$(TARGET)
SDDEVICE ?=

ifeq ($(TARGET),fpga)
DEFCONFIG := $(CURDIR)/configs/fpga_defconfig
OPENSBI_DIR := $(CURDIR)/components/opensbi
CAPSTONE_EXTRA_DEFS += -DCAPSTONE_TARGET_FPGA
# The board firmware carries the kernel and the device tree inside fw_payload.bin
# (platform/fpga/ariane/objects.mk reads this). Override with LINUX_PAYLOAD= to build a bare fw_jump.
LINUX_PAYLOAD ?= 1
else
DEFCONFIG := $(CURDIR)/configs/qemu_capstone_defconfig
OPENSBI_DIR := $(CURDIR)/components/opensbi
# The QEMU-private debug counters stay compiled into the QEMU monitor, as they always were.
CAPSTONE_EXTRA_DEFS += -DCAPSTONE_TARGET_QEMU -DCAPSTONE_DEBUG_ENABLE
LINUX_PAYLOAD ?=
endif

# The generated monitor assembly is per TARGET (the defines above select per-target code), and the
# two targets share one wrapper directory, so the .c.S must be regenerated when the TARGET changes,
# not only when a source changes. This stamp records the defines the current .c.S was made with.
CAPSTONE_DEFS_STAMP := $(OPENSBI_DIR)/lib/sbi/.capstone-defs
$(shell mkdir -p $(dir $(CAPSTONE_DEFS_STAMP)); [ "$$(cat $(CAPSTONE_DEFS_STAMP) 2>/dev/null)" = "$(CAPSTONE_EXTRA_DEFS)" ] || echo "$(CAPSTONE_EXTRA_DEFS)" > $(CAPSTONE_DEFS_STAMP))

ifeq ($(LINUX_PAYLOAD),1)
export LINUX_PAYLOAD=1
endif

CAPSTONE_S_OUTPUT = $(OPENSBI_DIR)/lib/sbi/sbi_capstone_dom.c.S \
		$(OPENSBI_DIR)/lib/sbi/capstone_int_handler.c.S
CAPSTONE_S_INCLUDE = $(OPENSBI_DIR)/lib/sbi/capstone-sbi
# THE HEADERS COUNT TOO. Adding only the .c above closed half the hole: sbi_capstone.h carries
# CAPSTONE_MAX_REGION_N, CAPSTONE_MAX_DOM_N and the error codes, so a header-only edit still left
# the generated assembly -- and the board firmware -- stale while the build reported success.
#
# That cost a boot on 2026-08-18. The region-table limit was raised in the header, the firmware
# relinked, the fresh timestamp looked convincing, and the board still enforced the OLD limit;
# the change was nearly written off as ineffective before the generated asm was checked. Using a
# wildcard rather than naming the header keeps this correct when another one is added.
CAPSTONE_S_INPUT = $(OPENSBI_DIR)/lib/sbi/capstone-sbi/sbi_capstone.c \
		$(wildcard $(OPENSBI_DIR)/lib/sbi/capstone-sbi/*.h)

BR = LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" -j $(JOBS)

.PHONY: all setup build build-vanilla check-cc clean flash-sdcard format-sd

all:

# Initialise the submodules only when they are ABSENT. `git submodule update` on an initialised
# tree checks every nested checkout back out at the recorded SHA, which silently discards
# in-progress monitor work that lives on a branch there.
setup:
	mkdir -p overlay
	@for d in buildroot components/opensbi; do \
		if [ ! -e "$$d/.git" ]; then echo "initialising submodules (first setup)"; git submodule update --init --recursive; break; fi; \
	done
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot defconfig BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" BR2_DEFCONFIG="$(DEFCONFIG)" O="$(CONFIG_PATH)" -j $(JOBS)

# The monitor compiler is REQUIRED and NAMED. It has no default because a relative default
# (../capstone-c) resolved to a different compiler depending on which checkout the command ran
# in, and a monitor built by an unrecorded compiler has cost a boot-hang investigation before.
# Every build prints the compiler it used so a firmware can be traced back to it.
check-cc:
	@test -n "$(CAPSTONE_CC_PATH)" || { echo "CAPSTONE_CC_PATH is required: the capstone-c checkout that regenerates the monitor assembly (e.g. CAPSTONE_CC_PATH=\$$(realpath ../../../capstone-c))" >&2; exit 1; }
	@test -d "$(CAPSTONE_CC_PATH)" || { echo "CAPSTONE_CC_PATH=$(CAPSTONE_CC_PATH) is not a directory" >&2; exit 1; }
	@echo "target=$(TARGET) out=$(CONFIG_PATH) capstone-c=$(CAPSTONE_CC_PATH) ($$(git -C '$(CAPSTONE_CC_PATH)' rev-parse --short HEAD 2>/dev/null || echo 'not a git checkout'))"

build-vanilla:
	$(BR) $(A)
	if [ -n "$(A)" ]; then \
		$(BR); \
	fi

# ORDERING. Most A= targets FEED the rootfs -- modcapstone-rebuild installs capstone.ko and
# capstone-test-domains installs *.dom into TARGET_DIR -- so they must run BEFORE the bare
# pass rolls the cpio and builds Image. opensbi-rebuild is the exception: it CONSUMES
# images/Image (platform/fpga/ariane/objects.mk sets FW_PAYLOAD_PATH=../../images/Image), so
# running it first links the PREVIOUS generation's kernel into fw_payload.bin.
#
# That is why the FPGA recipe has to invoke `make build A=opensbi-rebuild` TWICE to converge:
# the first run links a stale Image and the second happens to link the right one because the
# two generations are identical. Measured 2026-07-31 on this tree: fw_payload.bin was written
# 30.8 s BEFORE the images/Image it claims to embed. It is also how a board session once ran
# a day-old initramfs and answered `not found` (exit 127), which read as a domain failure.
#
# So: for opensbi-rebuild ONLY, run the bare pass first and relink after. The firmware then
# embeds the final Image by construction rather than by the accident that Image1 == Image2,
# and one full buildroot pass per iteration disappears. Deliberately NOT a blanket swap --
# that would ship a stale .ko/.dom and relocate the wrong-binary bug onto /test-domains/.
# A payload firmware embeds images/Image and images/caplifive.dtb, so on a FRESH output directory
# the kernel must exist before OpenSBI links (buildroot orders opensbi before linux unless its
# own LINUX_PAYLOAD knob is used, which this tree does not). The old tree never hit this because
# its images/ already existed; a new build-$(TARGET)/ does not.
build: check-cc $(CAPSTONE_S_OUTPUT)
	if [ "$(LINUX_PAYLOAD)" = "1" ] && [ ! -f "$(CONFIG_PATH)/images/Image" ]; then \
		$(BR) linux; \
	fi
	if [ "$(A)" = "opensbi-rebuild" ]; then \
		$(BR); \
		$(BR) $(A); \
	else \
		$(BR) $(A); \
		if [ -n "$(A)" ]; then \
			$(BR); \
		fi; \
	fi

ifeq ($(TARGET),fpga)
FW     := $(CONFIG_PATH)/images/fw_payload.bin
KERNEL := $(CONFIG_PATH)/images/Image
FDT    := $(CONFIG_PATH)/images/caplifive.dtb

# Partition layout (sector = 512 bytes)
FW_SECTORSTART := 2048
FW_SECTORSIZE  = $(shell ls -l --block-size=512 $(FW) | awk '{print $$5}')
FW_SECTOREND   = $(shell echo $$(($(FW_SECTORSTART) + $(FW_SECTORSIZE))))

KERNEL_SECTORSTART := 1048576# 512MB offset

SDDEVICE_PART1 = $(shell lsblk $(SDDEVICE) -no PATH | head -2 | tail -1)
SDDEVICE_PART2 = $(shell lsblk $(SDDEVICE) -no PATH | head -3 | tail -1)

flash-sdcard: format-sd
	@echo "Flashing fw_jump.bin to partition 1 ($(SDDEVICE_PART1))..."
	sudo dd if=$(FW) of=$(SDDEVICE_PART1) bs=1M status=progress oflag=sync

format-sd:
	@test -b "$(SDDEVICE)" || (echo 'SDDEVICE must be set and valid, e.g. SDDEVICE=/dev/sda' && exit 1)
	@echo "Formatting $(SDDEVICE)..."
	sudo sgdisk --clear -g \
		--new=1:$(FW_SECTORSTART):$(FW_SECTOREND) --typecode=1:3000 \
		--new=2:$(KERNEL_SECTORSTART):0 --typecode=2:8300 \
		$(SDDEVICE)
else
flash-sdcard format-sd:
	@echo "$@ is an FPGA target (TARGET=fpga)" >&2; exit 1
endif

# THE MONITOR SOURCE IS A PREREQUISITE, NOT JUST THE ONE-LINE WRAPPER.
#
# sbi_capstone_dom.c is a single `#include "capstone-sbi/sbi_capstone.c"`. With the bare
# %.c.S:%.c rule, make compared the generated assembly against that wrapper only, so every edit
# to the monitor left the .c.S -- and therefore the BOARD FIRMWARE -- silently stale while the
# build reported success.
#
# This had already fired and gone unnoticed: on 2026-08-15 the flashed firmware was found to be
# built from Aug-6 assembly while sbi_capstone.c was Aug-12, so the DBAS/DENT trace markers added
# for a board investigation were absent from every boot that was supposed to print them. The
# identical defect existed in the QEMU checkout's Makefile and cost a full misdiagnosis loop
# there the day before.
#
# $< is used below rather than $^ because the recipe must pass ONLY the wrapper to the compiler;
# the extra prerequisite is for dependency tracking, not an input file. A FAILED regen deletes
# its output: parse the wrapper in place first when editing the monitor, or the tree cannot relink
# until the source parses again.
$(CAPSTONE_S_OUTPUT):%.c.S:%.c $(CAPSTONE_S_INPUT) $(CAPSTONE_DEFS_STAMP)
	cd "$(CAPSTONE_CC_PATH)" && if ! /bin/sh -c 'cargo run -- --abi capstone $< -- -I"$(CAPSTONE_S_INCLUDE)" -D__riscv_xlen=64 $(CAPSTONE_EXTRA_DEFS) > "$@"'; then \
		rm -f "$@"; \
		echo "Compilation error. Make sure you supply the correct Capstone-C compiler directory path in CAPSTONE_CC_PATH" >&2; \
		false; \
	fi

clean:
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot clean O="$(CONFIG_PATH)"
