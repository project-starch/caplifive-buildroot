CAPSTONE_TEST_DOMAINS_VERSION = 1.0
CAPSTONE_TEST_DOMAINS_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-test-domains/src
CAPSTONE_TEST_DOMAINS_SITE_METHOD = local

define CAPSTONE_TEST_DOMAINS_BUILD_CMDS
	$(MAKE) -C '$(@D)' CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define CAPSTONE_TEST_DOMAINS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/test.dom' '$(TARGET_DIR)/test-domains/test.dom'
endef

$(eval $(generic-package))
