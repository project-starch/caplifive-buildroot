## Captainer Buildroot

This repo provides scripts and config files for building
a Captainer system that can run on [Capstone-QEMU](https://github.com/project-starch/capstone-qemu).

### Dependencies

Make sure your environment meets the [requirements of
Buildroot](https://buildroot.org/downloads/manual/manual.html#_about_buildroot).
Also make sure that you have the Rust toolchain and have already
built
[Capstone-C](https://github.com/jasonyu1996/capstone-c/tree/riscv-wip).

### Build Instructions

Initially, simply run the following

    make setup
    make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory>

You will be able to find the built images in `./build/images`, ready
to be fed to Capstone-QEMU.

If you have made changes to OpenSBI, sync and rebuild with

    make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=opensbi-rebuild

Similarly, to sync changes to the Linux kernel and rebuild, use

    make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=linux-rebuild

