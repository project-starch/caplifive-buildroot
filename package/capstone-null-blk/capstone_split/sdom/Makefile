obj-m += nullb_split.smode.o
ccflags-y := -Wno-declaration-after-statement -std=gnu99

.PHONY: all clean

all:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(TARGET_CROSS) modules

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(TARGET_CROSS) clean
