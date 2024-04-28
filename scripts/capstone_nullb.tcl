set timeout -1

cd $::env(CAPSTONE_QEMU_PATH)

spawn sh start.sh

interact {
    -o "login: " {
        send "root\r"
        return
    }
}

interact {
    -o "# " {
        send "insmod /capstone.ko\r"
        send "modprobe configfs\r"
        return
    }
}

interact {
    -o "# " {
        send "/clear-counters\r"
        send "/null_blk.user\r"
        send "insmod /nullb/capstone_split/null_blk.ko\r"
        send "echo \"hello world\" | dd of=/dev/nullb0 bs=1024 count=10\r"
        send "dd if=/dev/nullb0 bs=1024 count=10 | hexdump -C\r"
        # send "rmmod null_blk\r"
        send "/print-counters\r"
        send "poweroff -f\r"
    }
}
