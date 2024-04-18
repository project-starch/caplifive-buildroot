# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

includes += include_directories('../shared')
includes += include_directories('cmdline')

allow_experimental_apis = true
sources = files(
        '../shared/commons.c',
        '../shared/libcapstone.c',
        'cmdline/server_cmdline.c',
        'args_parser.c',
        'init.c',
        'main.c'
)
