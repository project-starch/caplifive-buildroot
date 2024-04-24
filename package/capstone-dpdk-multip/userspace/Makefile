SHELL := /bin/bash
DPDK_DIR := $(CURDIR)/dpdk-stable-22.11.3
EXAMPLES_DIR = $(DPDK_DIR)/examples
MULTI_PROCESS_DIR := $(EXAMPLES_DIR)/multip_server

.PHONY: all clean

all: setup_build
	rm -rf $(MULTI_PROCESS_DIR) && \
	cp -r multip_server $(MULTI_PROCESS_DIR) && \
	cd $(DPDK_DIR)/riscv-build && \
	meson configure -Dexamples=multip_server && \
	ninja

clean:
	rm -rf $(DPDK_DIR)

setup_build:
	@if [ ! -d "$(DPDK_DIR)" ]; then \
		wget https://fast.dpdk.org/rel/dpdk-22.11.3.tar.xz && \
		tar -xf dpdk-22.11.3.tar.xz && \
		rm dpdk-22.11.3.tar.xz && \
		rm ${EXAMPLES_DIR}/meson.build && \
		cp meson.build ${EXAMPLES_DIR}/meson.build && \
		cp -r multip_server $(MULTI_PROCESS_DIR) && \
		cd ${DPDK_DIR} && \
		meson setup riscv-build --cross-file $(DPDK_DIR)/config/riscv/riscv64_linux_gcc; \
	fi
