obj-m += nullb_split.smode.o
ccflags-y := -Wno-declaration-after-statement -std=gnu99

.PHONY: all clean

all:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(TARGET_CROSS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" nullb_split.smode.o
	$(LD) -pie --no-dynamic-linker -T sdom.lds -e init_module -o nullb_split.smode.ko nullb_split.smode.o

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(TARGET_CROSS) clean
