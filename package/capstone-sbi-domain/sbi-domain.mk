CAPSTONE_SBI_DOMAIN_VERSION = 1.0
CAPSTONE_SBI_DOMAIN_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-sbi-domain
CAPSTONE_SBI_DOMAIN_SITE_METHOD = local

define CAPSTONE_SBI_DOMAIN_BUILD_CMDS
	$(MAKE) -C '$(@D)' CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define CAPSTONE_SBI_DOMAIN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/'*.dom -t '$(TARGET_DIR)/test-domains/'
endef

$(eval $(generic-package))
