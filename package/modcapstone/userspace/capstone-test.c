#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <elf.h>
#include <fcntl.h>
#include "../include/capstone.h"

int dev_fd;

const unsigned char ELF_HEADER_MAGIC[4] = {
    ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3
};

static void open_device() {
    dev_fd = open(CAPSTONE_DEV_PATH, O_NONBLOCK);
    if (dev_fd < 0) {
        fprintf(stderr, "Failed to open the Capstone device file.\n");
        exit(dev_fd);
    }
}

int main(int argc, char** argv) {
    int retval = 0;
    if (argc < 2) {
        fprintf(stderr, "Please provide the domain file name!\n");
        return 1;
    }
    char* file_name = argv[1];

    open_device();

    FILE *elf_file = fopen(file_name, "rb");
    if (elf_file == NULL) {
        fprintf(stderr, "Failed to open the file.\n");
        retval = 1;
        goto clean_up_dev;
    }

    Elf64_Ehdr elf_header;
    if (!fread(&elf_header, sizeof(elf_header), 1, elf_file)) {
        fprintf(stderr, "Failed to read the ELF header.\n");
        retval = 1;
        goto clean_up_file;
    }

    if (strncmp(ELF_HEADER_MAGIC, elf_header.e_ident, sizeof(ELF_HEADER_MAGIC)))
    {
        fprintf(stderr, "Not an ELF file.\n");
        retval = 1;
        goto clean_up_file;
    }

    if (elf_header.e_machine != EM_RISCV) {
        fprintf(stderr, "Not for RISC-V.\n");
        retval = 1;
        goto clean_up_file;
    }

    printf("Ok, good file.\n");

clean_up_file:

    fclose(elf_file);


    // int retval = ioctl(dev_fd, IOCTL_HELLO, 0);

    // if (retval < 0) {
    //     fprintf(stderr, "Errors in ioctl: %d\n", retval);
    // }

clean_up_dev:

    retval = close(dev_fd);

    if (retval < 0) {
        fprintf(stderr, "Failed to close the device file");
    }

    return retval;
}
