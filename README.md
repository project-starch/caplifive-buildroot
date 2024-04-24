# Captainer Buildroot and Kernel Modules

This repo provides scripts and config files for building
a Captainer system that can run on [Capstone-QEMU](https://github.com/project-starch/capstone-qemu).

Also included is a Linux kernel module for Captainer, along with a
corresponding test programs and case studies.

## Dependencies

Make sure your environment meets the [requirements of
Buildroot](https://buildroot.org/downloads/manual/manual.html#_about_buildroot).
Also make sure that you have the Rust toolchain and have already
built
[Capstone-C](https://github.com/jasonyu1996/capstone-c/) and [Capstone-Qemu](https://github.com/project-starch/capstone-qemu).

## Build Instructions

Initially, simply run the following

```sh
make setup
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory>
```

You will be able to find the built images in `./build/images`, ready
to be fed to [Capstone-QEMU](https://github.com/project-starch/capstone-qemu).

If you have made changes to OpenSBI, sync and rebuild with

> Please manually delete `sbi_capstone_dom.c.S` and `capstone_int_handler.c.S` in `components/opensbi/lib/sbi` before rebuild.

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=opensbi-rebuild
```

Similarly, to sync changes to the Linux kernel and rebuild, use

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=linux-rebuild
```

For the kernel module or the test program,

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=modcapstone-rebuild
```

You can place the files you want to include in the rootfs in
`./overlay`.

## Quick Start

Pass the following arguments to [Capstone-QEMU](https://github.com/project-starch/capstone-qemu) in order to run all case studies:

> Alternatively, you can simply run the `start.sh` script in Capstone-QEMU repo.

```
-M virt-capstone -m 8G -nographic
-bios ../captainer-buildroot/build/images/fw_jump.elf
-kernel ../captainer-buildroot/build/images/Image
-append 'root=/dev/vda ro'
-drive file=../captainer-buildroot/build/images/rootfs.ext2,format=raw,id=hd0
-netdev user,id=net0,hostfwd=tcp::60022-:22
-device virtio-blk-device,drive=hd0
-chardev stdio,mux=on,id=ch0,signal=on
-mon chardev=ch0,mode=readline
-serial chardev:ch0
-device e1000,netdev=net0
-cpu rv64,sstc=false,h=false
```

where `<path>` is the path to the local working directory of this repo.

Log in using `root`.

Both the kernel module and the test program are located at `/`.
To install or uninstall the kernel module,

```sh
insmod /capstone.ko # install
rmmod capstone  # uninstall
```

After the kernel module is installed, you can run the test program

```sh
/capstone-test.user <test domain ELF file> [<number of times to call the domain (default: 1)>]
```

The `/run-test` script does all the operations above.

```sh
/run-test <test domain name> [<number of times to call the domain (default: 1)>]
```

The test domain name does not include the `.dom` suffix.

## Case Study: Block Device Driver 

### Build Instructions

Build the null block device driver:

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=capstone-null-blk-build
```

Build the user space setup program:

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=modcapstone-rebuild
```

### Quick Start

> TL;DR: Run `CAPSTONE_QEMU_PATH=<path-to-capstone-qemu> expect capstone_nullb.tcl` in `scripts` to see the results.

Install the `capstone` and `configfs` kernel modules:

```sh
modprobe configfs
insmod /capstone.ko
```

Run the user space setup program:

```sh
/null_blk.user
```

Install the null block device driver:

```sh
insmod /nullb/capstone_split/null_blk.ko
```

Then the null block device `/dev/nullb0` is ready to be used. You can write to the driver:

```sh
echo "hello world" | dd of=/dev/nullb0 bs=1024 count=10
```

Or read from the driver:

```sh
dd if=/dev/nullb0 bs=1024 count=10 | hexdump -C
```

If you want to remove the device, simple run:

```sh
rmmod null_blk
```

## Case Study: DPDK Multi-process Sample Application

### Dependencies

Make sure your environment meets the [requirements of DPDK](https://doc.dpdk.org/guides/linux_gsg/sys_reqs.html).
If you are on Ubuntu/Debian, the following packages are suggested to be installed:

```sh
sudo apt install meson ninja-build crossbuild-essential-riscv64 python3-pyelftools
```

### Build Instructions

Build the DPDK multi-process sample application:

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=capstone-dpdk-multip-rebuild
```

### Quick Start

> TL;DR: Run `CAPSTONE_QEMU_PATH=<path-to-capstone-qemu> expect capstone_dpdk_multip.tcl` in `scripts` to see the results.

Install the `capstone` kernel module:

```sh
insmod /capstone.ko
```

Mount the `hugetlbfs` file system and set the number of huge pages to 2048:

```sh
mkdir /mnt/huge
mount -t hugetlbfs nodev /mnt/huge
echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

Start the `dpdk-chat_server`:

```sh
/dpdk/dpdk-chat_server -l 0 -n 4 -- -p 3 -n <nr_of_domains> # in this case study, set nr_of_domains to 2
```

While `dpdk-chat_server` is running, the internal command line provides commands such as `send_to_domain` and `receive_from_domain`.

You can now communicate with any initialized domain based on its ID (which starts from 0) with these 2 commands in the internal command line, e.g.,

```sh
send_to_domain 0 5912 214 9124124 94 2014 1294 1204 29
send_to_domain 1 592 5012 59 10 27 249 2419 20
```

And then,

```sh
receive_from_domain 0
receive_from_domain 1
```

## Case Study: Web Server in Nested Domains

### Build Instructions

Build the web server frontend:

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=modcapstone-rebuild
```

Build the web server middle end and external scripts:

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=capstone-nested-enclave-build
```

### Quick Start

> TL;DR: Run `CAPSTONE_QEMU_PATH=<path-to-capstone-qemu> expect capstone_nested_enclave.tcl` in `scripts` to see the results.

Install the `capstone` kernel module:

```sh
insmod /capstone.ko
```

Start the web server at the background:

```sh
/miniweb_frontend.user &
```

Now the web server is ready to be used. You can now use `wget` to send a GET request to the web server, e.g.,

```sh
busybox wget -O - http://localhost:8888/index.html
busybox wget -O - http://localhost:8888/null.html # 404 response
busybox wget -O - http://localhost:8888/register.html
```

You can also send a POST request and specify an external script to handle such request, e.g.,

```sh
busybox wget --post-data "name=Alex&email=alex@email.com" -O - http://localhost:8888/cgi/cgi_register_success.dom
```

```sh
busybox wget --post-data "name=Bob&email=bob@email.com" -O - http://localhost:8888/cgi/cgi_register_success.dom
```

```sh
busybox wget --post-data "name=Alex&email=alex@email.com" -O - http://localhost:8888/cgi/cgi_register_fail.dom
```
