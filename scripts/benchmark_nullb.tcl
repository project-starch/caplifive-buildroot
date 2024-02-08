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
        send "/null_blk.user\r"
        send "insmod /nullb/capstone_split/null_blk.ko\r"
        send "/benchmark/null-blk | tee /benchmark-nullb.log\r"
        send "/print-counters\r"
        send "poweroff -f\r"
    }
}
