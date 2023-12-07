## Captainer Buildroot and Kernel Module

This repo provides scripts and config files for building
a Captainer system that can run on [Capstone-QEMU](https://github.com/project-starch/capstone-qemu).

Also included is a Linux kernel module for Captainer, along with a
corresponding test program.

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

You can place the files you want to include in the rootfs in
`./overlay`.

### Usage

Pass the following arguments to Capstone-QEMU:

    -M virt-capstone -nographic
    -bios <path>/build/images/fw_jump.elf
    -kernel <path>/build/images/Image
    -append 'root=/dev/vda ro'
    -drive file=<path>/build/images/rootfs.ext2,format=raw,id=hd0
    -device virtio-blk-device,drive=hd0
    -chardev stdio,mux=on,id=ch0,signal=on
    -mon chardev=ch0,mode=readline
    -serial chardev:ch0
    -serial chardev:ch0
    -cpu rv64,sstc=false,h=false

where `<path>` is the path to the local working directory of this repo.


Log in using `root`.

Both the kernel module and the test program are located at `/`.
To install or uninstall the kernel module,

    modprobe capstone       # install
    modprobe -r capstone    # uninstall

After the kernel module is installed, you can run the test program

    /capstone-test
