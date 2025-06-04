SHELL := /bin/bash
BUILDROOT_EXTERNAL = $(CURDIR)
DEFCONFIG = $(CURDIR)/configs/fpga_defconfig
CONFIG_PATH = $(CURDIR)/build
CAPSTONE_S_OUTPUT = $(CURDIR)/components/opensbi/lib/sbi/sbi_capstone_dom.c.S \
		$(CURDIR)/components/opensbi/lib/sbi/capstone_int_handler.c.S
CAPSTONE_S_INPUT = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi/sbi_capstone.c
CAPSTONE_S_INCLUDE = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi
PLATFORM := fpga/ariane
SDDEVICE ?=

.PHONY: all flash-sdcard format-sd setup build clean

all:

setup:
	mkdir -p overlay
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot defconfig BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" BR2_DEFCONFIG="$(DEFCONFIG)" O="$(CONFIG_PATH)" -j $(shell nproc)

build: $(CAPSTONE_S_OUTPUT)
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" $(A) -j $(shell nproc)
	if [ -n "$(A)" ]; then \
		LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" -j $(shell nproc); \
	fi

QEMU_BUILD = $(CURDIR)/build/images
FW :=$(CURDIR)/build/images/fw_payload.bin
KERNEL :=$(CURDIR)/build/images/Image
FDT    :=$(CURDIR)/build/images/caplifive.dtb


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

$(CAPSTONE_S_OUTPUT):%.c.S:%.c
	cd "$(CAPSTONE_CC_PATH)" && if ! /bin/sh -c 'cargo run -- --abi capstone $^ -- -I"$(CAPSTONE_S_INCLUDE)" -D__riscv_xlen=64 > "$@"'; then \
		rm -f "$@"; \
		echo "Compilation error. Make sure you supply the correct Capstone-C compiler directory path in CAPSTONE_CC_PATH" >&2; \
		false; \
	fi

clean:
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot clean O="$(CONFIG_PATH)"

