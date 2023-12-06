MODCAPSTONE_VERSION = 1.0
MODCAPSTONE_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/modcapstone/src
MODCAPSTONE_SITE_METHOD = local

define MODCAPSTONE_BUILD_CMDS
	$(MAKE) -C '$(@D)' LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)' CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
endef

define MODCAPSTONE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/capstone.ko' '$(TARGET_DIR)/capstone.ko'
endef

$(eval $(kernel-module))
$(eval $(generic-package))

