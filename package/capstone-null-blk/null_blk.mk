CAPSTONE_NULL_BLK_VERSION = 1.0
CAPSTONE_NULL_BLK_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-null-blk
CAPSTONE_NULL_BLK_SITE_METHOD = local

define CAPSTONE_NULL_BLK_BUILD_CMDS
	$(MAKE) -C '$(@D)'/baseline LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/baseline CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
	$(MAKE) -C '$(@D)'/module_split LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/module_split CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
	$(MAKE) -C '$(@D)'/capstone_split/module LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/capstone_split/module CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
	$(MAKE) -C '$(@D)'/capstone_split/sdom LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/capstone_split/sdom CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
	$(MAKE) -C '$(@D)'/module_debug LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/module_debug CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
endef

define CAPSTONE_NULL_BLK_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/baseline/null_blk.ko' '$(TARGET_DIR)/nullb/baseline/null_blk.ko'
	$(INSTALL) -D -m 0755 '$(@D)/debug/debug_baseline.sh' '$(TARGET_DIR)/nullb/baseline/debug_baseline.sh'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/null_blk.ko' '$(TARGET_DIR)/nullb/module_split/null_blk.ko'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/nullb_split.ko' '$(TARGET_DIR)/nullb/module_split/nullb_split.ko'
	$(INSTALL) -D -m 0755 '$(@D)/debug/debug_module_split.sh' '$(TARGET_DIR)/nullb/module_split/debug_module_split.sh'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/module/null_blk.ko' '$(TARGET_DIR)/nullb/capstone_split/null_blk.ko'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/sdom/nullb_split.smode.ko' '$(TARGET_DIR)/nullb/capstone_split/nullb_split.smode.ko'
endef

$(eval $(generic-package))
