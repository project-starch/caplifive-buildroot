CAPSTONE_NULL_BLK_VERSION = 1.0
CAPSTONE_NULL_BLK_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-null-blk
CAPSTONE_NULL_BLK_SITE_METHOD = local

define CAPSTONE_NULL_BLK_BUILD_CMDS
	$(MAKE) -C '$(@D)'/driver LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/driver CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
endef

define CAPSTONE_NULL_BLK_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/driver/null_blk.ko' '$(TARGET_DIR)/null_blk.ko'
endef

$(eval $(generic-package))
