# Package override file for TARGET=qemu (selected by BR2_PACKAGE_OVERRIDE_FILE in configs/qemu_capstone_defconfig).
# The QEMU stand-in builds the pinned components/linux kernel and, transitionally, its own checkout
# of the OpenSBI wrapper (see Makefile: OPENSBI_DIR).
LINUX_OVERRIDE_SRCDIR = $(BR2_EXTERNAL_CAPSTONE_PATH)/components/linux
OPENSBI_OVERRIDE_SRCDIR = $(BR2_EXTERNAL_CAPSTONE_PATH)/components/opensbi-qemu
