include $(sort $(wildcard $(BR2_EXTERNAL_CAPSTONE_PATH)/package/*/*.mk))

# The FPGA core has no C extension in the configuration the board runs: modules and domains built
# with compressed instructions took misaligned-access faults (commit cfa3d49). Keyed on the
# defconfig's OpenSBI platform rather than on an environment variable, so the flags follow the
# TARGET's configuration and a QEMU build can never pick them up by accident. Applied once here
# instead of once per package.
ifeq ($(call qstrip,$(BR2_TARGET_OPENSBI_PLAT)),fpga/ariane)
EXTRA_CFLAGS += -march=rv64g -mabi=lp64d
endif
