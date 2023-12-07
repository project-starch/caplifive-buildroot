CAPSTONE_TEST_VERSION = 1.0
CAPSTONE_TEST_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-test/src
CAPSTONE_TEST_SITE_METHOD = local

define CAPSTONE_TEST_BUILD_CMDS
	$(MAKE) -C '$(@D)' PWD='$(@D)' CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define CAPSTONE_TEST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/capstone-test' '$(TARGET_DIR)/capstone-test'
endef

$(eval $(generic-package))

