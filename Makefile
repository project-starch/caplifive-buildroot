SHELL := /bin/bash
BUILDROOT_EXTERNAL = $(CURDIR)
DEFCONFIG ?= $(CURDIR)/configs/fpga_defconfig
CONFIG_PATH = $(CURDIR)/build
CAPSTONE_S_OUTPUT = $(CURDIR)/components/opensbi/lib/sbi/sbi_capstone_dom.c.S \
		$(CURDIR)/components/opensbi/lib/sbi/capstone_int_handler.c.S
CAPSTONE_S_INPUT = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi/sbi_capstone.c
CAPSTONE_S_INCLUDE = $(CURDIR)/components/opensbi/lib/sbi/capstone-sbi
PLATFORM := fpga/ariane
SDDEVICE ?=

 LINUX_PAYLOAD ?=
 ifeq ($(LINUX_PAYLOAD),1)
	 export LINUX_PAYLOAD=1
 endif

.PHONY: all flash-sdcard format-sd setup build build-vanilla clean

all:

setup:
	mkdir -p overlay
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot defconfig BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" BR2_DEFCONFIG="$(DEFCONFIG)" O="$(CONFIG_PATH)" -j $(shell nproc)


build-vanilla:
	LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" $(A) -j $(shell nproc)
	if [ -n "$(A)" ]; then \
		LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" -j $(shell nproc); \
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
build: $(CAPSTONE_S_OUTPUT)
	if [ "$(A)" = "opensbi-rebuild" ]; then \
		LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" -j $(shell nproc); \
		LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" $(A) -j $(shell nproc); \
	else \
		LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" $(A) -j $(shell nproc); \
		if [ -n "$(A)" ]; then \
			LD_LIBRARY_PATH="" $(MAKE) -C buildroot BR2_EXTERNAL="$(BUILDROOT_EXTERNAL)" O="$(CONFIG_PATH)" -j $(shell nproc); \
		fi; \
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

