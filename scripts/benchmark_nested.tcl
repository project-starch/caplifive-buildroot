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
        return
    }
}

interact {
    -o "# " {
        send "/miniweb_frontend.user &\r"
        return
    }
}

interact {
    -o "Waiting for incoming connections..." {
        send "/benchmark/nested-enclave\r"
        return
    }
}

interact {
    -o "# " {
        send "/print-counters\r"
        send "poweroff -f\r"
    }
}
