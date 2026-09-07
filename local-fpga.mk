# Package override file for TARGET=fpga (selected by BR2_PACKAGE_OVERRIDE_FILE in configs/fpga_defconfig).
# The board builds buildroot's own kernel; only OpenSBI comes from the tree.
OPENSBI_OVERRIDE_SRCDIR = $(BR2_EXTERNAL_CAPSTONE_PATH)/components/opensbi
