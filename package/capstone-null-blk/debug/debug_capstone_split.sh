#!/bin/sh

set -e

echo "install configfs"
modprobe configfs
echo "install capstone"
insmod /capstone.ko
echo "run /null_blk.user"
/null_blk.user
echo "install null_blk"
insmod ./null_blk.ko
echo "ls -l /dev | grep nullb"
ls -l /dev | grep nullb
