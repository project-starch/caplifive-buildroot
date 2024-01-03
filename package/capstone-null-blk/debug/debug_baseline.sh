#!/bin/sh

set -e

echo "install configfs"
modprobe configfs
echo "install null_blk"
insmod ./null_blk.ko
echo "ls -l /dev | grep nullb"
ls -l /dev | grep nullb
echo "dmesg | grep null_blk"
dmesg | grep null_blk
echo "write data to null_blk"
echo "hello world" | dd of=/dev/nullb0 bs=1024 count=10
echo "read data from null_blk"
dd if=/dev/nullb0 bs=1024 count=10 | hexdump -C
echo "remove null_blk"
rmmod null_blk
echo "ls -l /dev | grep nullb"
ls -l /dev | grep nullb
