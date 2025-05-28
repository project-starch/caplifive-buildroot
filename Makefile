SHELL := /bin/bash
BUILDROOT_EXTERNAL = $(CURDIR)
DEFCONFIG = $(CURDIR)/configs/basebr_defconfig
CONFIG_PATH = $(CURDIR)/build
CAPSTONE_S_OUTPUT = $(CURDIR)/components/opensbi/lib/sbi/sbi_capstone_dom.c.S \
		$(CURDIR)/components/opensbi/lib/sbi/capstone_int_handler.c.S
CAPSTONE_S_INPUT = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi/sbi_capstone.c
CAPSTONE_S_INCLUDE = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi
PLATFORM := fpga/ariane

.PHONY: all flash-sdcard format-sd setup build clean

all:

setup:
	mkdir -p overlay
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot defconfig BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" BR2_DEFCONFIG="$(DEFCONFIG)" O="$(CONFIG_PATH)"

build: $(CAPSTONE_S_OUTPUT)
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" $(A)
	if [ -n "$(A)" ]; then \
		LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)"; \
	fi

QEMU_BUILD = $(CURDIR)/build/images
FWJUMP = $(QEMU_BUILD)/fw_payload.bin
KERNEL = $(QEMU_BUILD)/Image
ROOTFS = $(QEMU_BUILD)/rootfs.ext2


SDDEVICE ?=


FWJUMP_SECTORSTART := 2048
FWJUMP_SECTORSIZE = $(shell ls -l --block-size=512 $(FWJUMP) | awk '{print $$5}')
FWJUMP_SECTOREND = $(shell echo $$(($(FWJUMP_SECTORSTART) + $(FWJUMP_SECTORSIZE)))) 


KERNEL_SECTORSTART := 1048576
KERNEL_SECTORSIZE = $(shell ls -l --block-size=512 $(KERNEL) | awk '{print $$5}')
KERNEL_SECTOREND = $(shell echo $$(($(KERNEL_SECTORSTART) + $(KERNEL_SECTORSIZE)))) 

ROOTFS_SECTORSTART := 2097152
ROOTFS_SECTORSIZE = $(shell ls -l --block-size=512 $(ROOTFS) | awk '{print $$5}')
ROOTFS_SECTOREND = $(shell echo $$(($(ROOTFS_SECTORSTART) + $(ROOTFS_SECTORSIZE)))) 

SDDEVICE_PART1 = $(shell lsblk $(SDDEVICE) -no PATH | head -2 | tail -1)
SDDEVICE_PART2 = $(shell lsblk $(SDDEVICE) -no PATH | head -3 | tail -1)
SDDEVICE_PART3 = $(shell lsblk $(SDDEVICE) -no PATH | head -4 | tail -1)


flash-sdcard: format-sd
	@echo "Flashing fw_jump.elf to partition 1 ($(SDDEVICE_PART1))..."
	sudo dd if=$(FWJUMP) of=$(SDDEVICE_PART1) bs=1M status=progress oflag=sync

	@echo "Flashing kernel Image to partition 2 ($(SDDEVICE_PART2))..."
	# sudo dd if=$(KERNEL) of=$(SDDEVICE_PART2) bs=1M status=progress oflag=sync

	@echo "Flashing rootfs.ext2 to partition 3 ($(SDDEVICE_PART3))..."
	sudo dd if=$(ROOTFS) of=$(SDDEVICE_PART3) bs=1M status=progress oflag=sync

format-sd:
	@test -b "$(SDDEVICE)" || (echo 'SDDEVICE must be set and valid, e.g. SDDEVICE=/dev/sda' && exit 1)
	@echo "Formatting $(SDDEVICE) for QEMU images..."
	sudo sgdisk --clear -g \
		--new=1:$(FWJUMP_SECTORSTART):$(FWJUMP_SECTOREND) --typecode=1:3000 \
		--new=2:$(ROOTFS_SECTORSTART):0 --typecode=2:8300 \
		$(SDDEVICE)

$(CAPSTONE_S_OUTPUT):%.c.S:%.c
	cd "$(CAPSTONE_CC_PATH)" && if ! /bin/sh -c 'cargo run -- --abi capstone $^ -- -I"$(CAPSTONE_S_INCLUDE)" -D__riscv_xlen=64 > "$@"'; then \
		rm -f "$@"; \
		echo "Compilation error. Make sure you supply the correct Capstone-C compiler directory path in CAPSTONE_CC_PATH" >&2; \
		false; \
	fi

clean:
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot clean O="$(CONFIG_PATH)"

