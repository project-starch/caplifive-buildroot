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
        send "mkdir -p /mnt/huge\r"
        send "mount -t hugetlbfs nodev /mnt/huge\r"
        send "echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages\r"
        return
    }
}

interact {
    -o "# " {
        send "/clear-counters\r"
        send "/dpdk-multip/dpdk-multip_server -l 0 -n 4 -- -p 3 -n 2 < /benchmark/dpdk_multip_commands\r"
        send "/print-counters\r"
        send "poweroff -f\r"
    }
}
