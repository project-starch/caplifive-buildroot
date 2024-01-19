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

For the kernel module or the test program,

    make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=modcapstone-rebuild

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

    /capstone-test <test domain ELF file> [<number of times to call the domain (default: 1)>]

The `/run-test` script does all the operations above.

    /run-test <test domain name> [<number of times to call the domain (default: 1)>]

The test domain name does not include the `.dom` suffix.

### DPDK case study usage

In the `capstone-test-domains` directory there are some files that are names using the following format `dpdk_client<#>.dom.c`. This is the format that allows the creation of new *client domains*. For different implementations of clients, changes to the primary process might be needed as well.

Build and run QEMU as mentioned in the [Build Instructions](#build-instructions) and [Usage](#usage) sections. Once QEMU is up and running need to the following:
* install the *capstone.ko* kernel drive, in order to allow captainer functionalities.
* run `mount -t hugetlbfs nodev /mnt/huge && echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages`
* go to the *root (/)* directory
* run `./dpdk-chat_server -l 0 -n 4 -- -p 3 -n <nr_of_domains>`
* while *dpdk-chat_server* is running, the internal command line provides commands such as *send_to_domain* or *receive_from_domain* that can communicate with any of the initialized domains based on their IDs (which usually start from 0)

The `dpdp_server` should be part of the *overlay* directory. If not, go the dpdk directory, build the apptainer image from there called *dpdk-cross-compile.def* and then run `./<name_of_the_sif_image> build` or `./<name_of_the_sif_image> rebuild` (if you already executed the build command). After the build copy the *dpdk-chat_server* executable from the *dpdk/dpdk-stable-22.11.3/riscv-build/examples* directory to the *overlay* directory.

### DPDK case study baseline usage

In order to test the DPDK baseline testing what is needed is QEMU to run with a number vcpus equal to the `number_of_secondary_processes + 1`. The *+1* is needed for the primary process. All DPDK processes use a core and they don't yeild.
After runnning QEMU, running the baseline is similar to the original DPDK functionality for [basic multi-process functionality](https://doc.dpdk.org/guides-22.11/sample_app_ug/multi_process.html). The name of the executables in our case is *dpdk-chat_server* for the primary process and *dpdk-chat_client* for the secondary process(es).

In orde to use some setup needs to be done beforehand, similar to the captainer version:
* install the *capstone.ko* kernel drive, in order to allow captainer functionalities.
* run `mount -t hugetlbfs nodev /mnt/huge && echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages`
