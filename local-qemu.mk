# Package override file for TARGET=qemu (selected by BR2_PACKAGE_OVERRIDE_FILE in configs/qemu_capstone_defconfig).
# The QEMU stand-in builds the pinned components/linux kernel; OpenSBI comes from the same
# components/opensbi as the board (platform/generic is the QEMU target).
LINUX_OVERRIDE_SRCDIR = $(BR2_EXTERNAL_CAPSTONE_PATH)/components/linux
OPENSBI_OVERRIDE_SRCDIR = $(BR2_EXTERNAL_CAPSTONE_PATH)/components/opensbi
