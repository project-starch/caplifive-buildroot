# 🌟 Caplifive Buildroot and Kernel Module

This repository provides scripts and configuration files for building a **[Caplifive](https://capstone.kisp-lab.org/) system** that can run on [Caplifive-QEMU](https://github.com/project-starch/caplifive-qemu).

It also includes a **Linux kernel module** for Caplifive, along with corresponding **test programs** and **case studies**.

> **Note:** The full system can be built using the build script provided in [caplifive-qemu](https://github.com/project-starch/caplifive-qemu).



## 🛠️ **Dependencies**

Before building, ensure you have the following repositories built (Alternatively, use the `build.sh` script in [Caplifive-QEMU](https://github.com/project-starch/caplifive-qemu/) to build the full system):

1. [Capstone-C](https://github.com/jasonyu1996/capstone-c/)
2. [Caplifive-QEMU](https://github.com/project-starch/caplifive-qemu/)



## 🏗️ **Build Instructions**

### Option 1: Local Build (Debian-based Machine)

To build on a local Debian-based machine, make sure you have locally built **Capstone-C** and **Caplifive-QEMU** using their respective local build scripts. Then, run the following command:

```bash
    ./local_build.sh
```

### Option 2: Docker Image Build

To build the Docker image, follow these steps:

1. First, build the **Capstone-C** and **Caplifive-QEMU** images (they are tagged as `capstone-c` and `qemu-build` if built using the default instructions).
2. If you've customized the names, make sure to change them in the `Dockerfile`.

3. Run the following command to build the full-system Docker image:

```bash
    docker build -t <tag> .
```



> You will be able to find the built images in `./build/images`, ready to be fed to [Caplifive-QEMU](https://github.com/project-starch/caplifive-qemu).



### ⚡ **Quick Notes:**
---
If you have made changes to OpenSBI, sync and rebuild with

**Please manually delete `sbi_capstone_dom.c.S` and `capstone_int_handler.c.S` in `components/opensbi/lib/sbi` before rebuild.**

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

You can place the files you want to include in the rootfs in `./overlay`.

## 🚀 **Quick Start**

1. You can run the `start.sh` script in the [Caplifive-QEMU](https://github.com/project-starch/caplifive-qemu) repository, or you can pass the following arguments to the qemu image:

```bash
-M virt-capstone -m 8G -nographic
-bios <path>/build/images/fw_jump.elf
-kernel <path>/build/images/Image
-append 'root=/dev/vda ro'
-drive file=<path>/build/images/rootfs.ext2,format=raw,id=hd0
-netdev user,id=net0,hostfwd=tcp::60022-:22
-device virtio-blk-device,drive=hd0
-chardev stdio,mux=on,id=ch0,signal=on
-mon chardev=ch0,mode=readline
-serial chardev:ch0
-device e1000,netdev=net0
-cpu rv64,sstc=false,h=false
# Where `<path>` is the local working directory of this repo.
```
---

2. Log in using the `root` account. Both the kernel module and test program can be found at `/`.

---

3. To install or uninstall the kernel module, run:

```bash
# Install the kernel module
insmod /capstone.ko
```
```bash
# Uninstall the kernel module
rmmod capstone
```
---

4. After installing the kernel module, run the test program with:

```bash
/capstone-test.user <test domain ELF file> [<number of times to call the domain (default: 1)>]
```

---

> Alternatively, use the `run-test` script to automate the entire process:

```bash
/run-test <test domain name> [<number of times to call the domain (default: 1)>]
```


## 📜 Case Study: Block Device Driver

### Build Instructions

Build the null block device driver:

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=capstone-null-blk-build
```

Build the user space setup program:

```sh
make build CAPSTONE_CC_PATH=<path-to-capstone-c-compiler-directory> A=modcapstone-rebuild
```

###  Quick Start

Inside the qemu image after logging in:

1. Install the `capstone` and `configfs` kernel modules:

```sh
modprobe configfs
insmod /capstone.ko
```
---

2. Run the user space setup program:

```sh
/null_blk.user
```

---

3. Install the null block device driver:

```sh
insmod /nullb/capstone_split/null_blk.ko
```
---

4. Then the null block device `/dev/nullb0` is ready to be used. You can write to the driver:

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

<!-- ## Case Study: DPDK Multi-Process Sample Application

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

> TL;DR: Run `CAPSTONE_QEMU_PATH=<path-to-caplifive-qemu> expect capstone_dpdk_multip.tcl` in `scripts` to see the results.

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
``` -->

## 📜 Case Study: Web Server in Nested Domains

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

Inside the qemu image after logging in:

1. Install the `capstone` kernel module:

```sh
insmod /capstone.ko
```
---

2. Start the web server at the background:

```sh
/miniweb_frontend.user &
```
---

3. Now the web server is ready to be used. You can now use `wget` to send a GET request to the web server, e.g.,

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
---

## 📊 Case Study Benchmarks

### Benchmark Generation

> Note: Expected benchmarks in `overlay/benchmark`.

Simply run the scripts in `overlay-sources` to randomly generate the benchmarks for each case study:

```sh
cd overlay-sources
./benchmark-nullb-gen.sh > ../overlay/benchmark/null-blk # for block device driver
./benchmark-nested-gen.sh > ../overlay/benchmark/nested-enclave # for web server in nested domains
```

### Benchmarking

Simply run the scripts in `scripts` directory (present in docker container as well as local machine) as below to run the benchmarks automatically:

```sh
cd scripts
# replace <<path-to-caplifive-qemu> to the real path on your machine
CAPSTONE_QEMU_PATH=<path-to-caplifive-qemu> expect benchmark_nullb.tcl
CAPSTONE_QEMU_PATH=<path-to-caplifive-qemu> expect benchmark_nested.tcl
```

### Benchmark Results

At the end of benchmarking, the results will be shown in the following format:

```
[CAPSTONE] CAPSTONE DEBUG COUNTERS
[CAPSTONE] counter[<index>] = <value>
[CAPSTONE] counter[<index>] = <value>
...
```

Each index corresponds to a profiling result in our benchmark, the correspondence relationship is shown in the table below:

| Index | Profiling Subject |
| :---: | :---------------: |
| 0 | Context switching (U-mode) |
| 1 | Context switching (S-mode) |
| 2 | Context switching (C/M-mode) |
| 3 | Interrupt handling |
| 4 | Memory access fault handling |
| 5 - 9 | Not used yet |
| 10 | Asynchronous sharing (bytes) |
| 11 | Asynchronous sharing (times) |
| 12 | Synchronous immutable borrowing (bytes) |
| 13 | Synchronous immutable borrowing (times) |
| 14 | Synchronous double transfer (bytes) |
| 15 | Synchronous double transfer (times) |
| 16 | Synchronous immutable transferred borrowing (bytes) |
| 17 | Synchronous immutable transferred borrowing (times) |
| 18 | Synchronous mutable transferred borrowing (bytes) |
| 19 | Synchronous mutable transferred borrowing (times) |
| 20 - 31 | Not used yet |
