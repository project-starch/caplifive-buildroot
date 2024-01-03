CAPSTONE_NULL_BLK_VERSION = 1.0
CAPSTONE_NULL_BLK_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-null-blk
CAPSTONE_NULL_BLK_SITE_METHOD = local

define CAPSTONE_NULL_BLK_BUILD_CMDS
	$(MAKE) -C '$(@D)'/baseline LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/baseline CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
	$(MAKE) -C '$(@D)'/module_split LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/module_split CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
endef

define CAPSTONE_NULL_BLK_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/baseline/null_blk.ko' '$(TARGET_DIR)/nullb/baseline/null_blk.ko'
	$(INSTALL) -D -m 0755 '$(@D)/debug/nullb_baseline.sh' '$(TARGET_DIR)/nullb/baseline/nullb_baseline.sh'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/null_blk.ko' '$(TARGET_DIR)/nullb/module_split/null_blk.ko'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/nullb_split.ko' '$(TARGET_DIR)/nullb/module_split/nullb_split.ko'
	$(INSTALL) -D -m 0755 '$(@D)/debug/nullb_module.sh' '$(TARGET_DIR)/nullb/module_split/nullb_module.sh'
endef

$(eval $(generic-package))
