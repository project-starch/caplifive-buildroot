CAPSTONE_RUST_VERSION = 1.0
CAPSTONE_RUST_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-rust/src
CAPSTONE_RUST_SITE_METHOD = local

define CAPSTONE_RUST_BUILD_CMDS
	$(MAKE) -C '$(@D)' CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define CAPSTONE_RUST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/'*.dom -t '$(TARGET_DIR)/rust/'
endef

$(eval $(generic-package))
