CAPSTONE_NESTED_ENCLAVE_VERSION = 1.0
CAPSTONE_NESTED_ENCLAVE_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-nested-enclave
CAPSTONE_NESTED_ENCLAVE_SITE_METHOD = local

define CAPSTONE_NESTED_ENCLAVE_BUILD_CMDS
	$(MAKE) -C '$(@D)'/baseline CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define CAPSTONE_NESTED_ENCLAVE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/baseline/miniweb' '$(TARGET_DIR)/nested/baseline/miniweb'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/cgi/register' '$(TARGET_DIR)/nested/baseline/cgi/register'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/www/index.html' '$(TARGET_DIR)/nested/baseline/www/index.html'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/404Response.txt' '$(TARGET_DIR)/nested/baseline/404Response.txt'
	$(INSTALL) -D -m 0755 '$(@D)/debug/debug_baseline.sh' '$(TARGET_DIR)/nested/baseline/debug_baseline.sh'
endef

$(eval $(generic-package))
