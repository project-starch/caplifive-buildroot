CAPSTONE_NESTED_ENCLAVE_VERSION = 1.0
CAPSTONE_NESTED_ENCLAVE_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-nested-enclave
CAPSTONE_NESTED_ENCLAVE_SITE_METHOD = local

define CAPSTONE_NESTED_ENCLAVE_BUILD_CMDS
	$(MAKE) -C '$(@D)'/baseline CC='$(TARGET_CC)' LD='$(TARGET_LD)'
	# $(MAKE) -C '$(@D)'/capstone_split/cgi CC='$(TARGET_CC)' LD='$(TARGET_LD)'
	$(MAKE) -C '$(@D)'/capstone_split/sdom LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/capstone_split/sdom CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
endef

define CAPSTONE_NESTED_ENCLAVE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/baseline/miniweb' '$(TARGET_DIR)/nested/baseline/miniweb'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/cgi/register_success' '$(TARGET_DIR)/nested/baseline/cgi/register_success'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/cgi/register_fail' '$(TARGET_DIR)/nested/baseline/cgi/register_fail'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/www/index.html' '$(TARGET_DIR)/nested/baseline/www/index.html'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/404Response.txt' '$(TARGET_DIR)/nested/baseline/404Response.txt'
	# $(INSTALL) -D -m 0755 '$(@D)/capstone_split/cgi/cgi_register_success.dom' '$(TARGET_DIR)/nested/capstone_split/cgi_register_success.dom'
	# $(INSTALL) -D -m 0755 '$(@D)/capstone_split/cgi/cgi_register_fail.dom' '$(TARGET_DIR)/nested/capstone_split/cgi_register_fail.dom'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/sdom/miniweb_backend.smode.ko' '$(TARGET_DIR)/nested/capstone_split/miniweb_backend.smode.ko'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/response/404Response.txt' '$(TARGET_DIR)/nested/capstone_split/404Response.txt'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/response/www/'*.html -t '$(TARGET_DIR)/nested/capstone_split/www/'
endef

$(eval $(generic-package))
