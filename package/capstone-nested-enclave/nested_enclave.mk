CAPSTONE_NESTED_ENCLAVE_VERSION = 1.0
CAPSTONE_NESTED_ENCLAVE_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-nested-enclave
CAPSTONE_NESTED_ENCLAVE_SITE_METHOD = local

define CAPSTONE_NESTED_ENCLAVE_BUILD_CMDS
	$(MAKE) -C '$(@D)'/baseline CC='$(TARGET_CC)' LD='$(TARGET_LD)'
	$(MAKE) -C '$(@D)'/capstone_split/cgi CC='$(TARGET_CC)' LD='$(TARGET_LD)'
	$(MAKE) -C '$(@D)'/capstone_split/sdom CC='$(TARGET_CC)' LD='$(TARGET_LD)'
	$(MAKE) -C '$(@D)'/module_split/modules LINUX_DIR='$(LINUX_DIR)' PWD='$(@D)'/module_split/modules CC='$(TARGET_CC)' LD='$(TARGET_LD)' \
		KERNEL_ARCH=$(KERNEL_ARCH) TARGET_CROSS=$(TARGET_CROSS)
	$(MAKE) -C '$(@D)'/module_split/userspace CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define CAPSTONE_NESTED_ENCLAVE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/baseline/miniweb' '$(TARGET_DIR)/nested/baseline/miniweb'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/cgi/register' '$(TARGET_DIR)/nested/baseline/cgi/register'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/www/index.html' '$(TARGET_DIR)/nested/baseline/www/index.html'
	$(INSTALL) -D -m 0755 '$(@D)/baseline/404Response.txt' '$(TARGET_DIR)/nested/baseline/404Response.txt'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/cgi/cgi_register_success.dom' '$(TARGET_DIR)/nested/capstone_split/cgi/cgi_register_success.dom'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/cgi/cgi_register_fail.dom' '$(TARGET_DIR)/nested/capstone_split/cgi/cgi_register_fail.dom'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/sdom/miniweb_backend.smode' '$(TARGET_DIR)/nested/capstone_split/miniweb_backend.smode'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/response/404Response.txt' '$(TARGET_DIR)/nested/capstone_split/404Response.txt'
	$(INSTALL) -D -m 0755 '$(@D)/capstone_split/response/www/'*.html -t '$(TARGET_DIR)/nested/capstone_split/www/'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/modules/miniweb_backend.ko' '$(TARGET_DIR)/nested/module_split/miniweb_backend.ko'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/modules/cgi_register_success.ko' '$(TARGET_DIR)/nested/module_split/cgi_register_success.ko'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/modules/cgi_register_fail.ko' '$(TARGET_DIR)/nested/module_split/cgi_register_fail.ko'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/userspace/miniweb_frontend' '$(TARGET_DIR)/nested/module_split/miniweb_frontend'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/response/404Response.txt' '$(TARGET_DIR)/nested/module_split/404Response.txt'
	$(INSTALL) -D -m 0755 '$(@D)/module_split/response/www/'*.html -t '$(TARGET_DIR)/nested/module_split/www/'
endef

$(eval $(generic-package))
