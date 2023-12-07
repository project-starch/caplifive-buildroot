#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "capstone.h"


int main() {
    int dev_fd = open(CAPSTONE_DEV_PATH, O_NONBLOCK);
    if (dev_fd < 0) {
        fprintf(stderr, "Failed to open the Capstone device file.\n");
        return dev_fd;
    }

    int retval = ioctl(dev_fd, IOCTL_HELLO, 0);

    if (retval < 0) {
        fprintf(stderr, "Errors in ioctl: %d\n", retval);
    }

    retval = close(dev_fd);

    if (retval < 0) {
        fprintf(stderr, "Failed to close the device file");
        return retval;
    }

    return 0;
}
