CAPSTONE_DPDK_MULTIP_VERSION = 1.0
CAPSTONE_DPDK_MULTIP_SITE = $(BR2_EXTERNAL_CAPSTONE_PATH)/package/capstone-dpdk-multip
CAPSTONE_DPDK_MULTIP_SITE_METHOD = local

define CAPSTONE_DPDK_MULTIP_BUILD_CMDS
	$(MAKE) -C '$(@D)/userspace'
	$(MAKE) -C '$(@D)/dom' CC='$(TARGET_CC)' LD='$(TARGET_LD)'
endef

define CAPSTONE_DPDK_MULTIP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 '$(@D)/userspace/dpdk-stable-22.11.3/riscv-build/examples/dpdk-chat_server' '$(TARGET_DIR)/dpdk/dpdk-chat_server'
	$(INSTALL) -D -m 0755 '$(@D)/userspace/dpdk-stable-22.11.3/riscv-build/examples/dpdk-chat_client' '$(TARGET_DIR)/dpdk/dpdk-chat_client'
	$(INSTALL) -D -m 0755 '$(@D)/dom/'*.dom -t '$(TARGET_DIR)/dpdk/'
endef

$(eval $(generic-package)) 
