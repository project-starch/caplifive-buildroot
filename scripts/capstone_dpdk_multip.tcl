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
        send "/dpdk-multip/dpdk-multip_server -l 0 -n 4 -- -p 3 -n 2\r"
        return
    }
}

interact {
    -o "server_cmdline > " {
        send "send_to_domain 0 5912 214 9124124 94 2014 1294 1204 29\r"
        send "send_to_domain 1 592 5012 59 10 27 249 2419 20\r"
        send "receive_from_domain 0\r"
        send "receive_from_domain 1\r"
        send "quit\r"
        return
    }
}

interact {
    -o "# " {
        send "/print-counters\r"
        send "poweroff -f\r"
    }
}
