MODCAPSTONE_VERSION = 1.0
MODCAPSTONE_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/modcapstone
MODCAPSTONE_SITE_METHOD = local

define MODCAPSTONE_BUILD_CMDS
	$(MAKE) -C '$(@D)'/module LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/module CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
	$(MAKE) -C '$(@D)'/userspace CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define MODCAPSTONE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/module/capstone.ko' '$(TARGET_DIR)/capstone.ko'
	$(INSTALL) -D -m 0755 '$(@D)/userspace/capstone-test' '$(TARGET_DIR)/capstone-test'
endef

$(eval $(kernel-module))
$(eval $(generic-package))

