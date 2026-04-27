obj-m += null_blk.o
ccflags-y := -Wno-declaration-after-statement -std=gnu99

EXTRA_CFLAGS += -D__CAPSTONE_DEBUG_FLAG__

.PHONY: all clean

all:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(TARGET_CROSS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(TARGET_CROSS) clean
