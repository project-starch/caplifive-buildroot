#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <fcntl.h>
#include "../include/capstone.h"

static int dev_fd;

static const unsigned char ELF_HEADER_MAGIC[4] = {
    ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3
};

static void open_device() {
    dev_fd = open(CAPSTONE_DEV_PATH, O_NONBLOCK);
    if (dev_fd < 0) {
        fprintf(stderr, "Failed to open the Capstone device file.\n");
        exit(dev_fd);
    }
}

static dom_id_t create_dom(void *code_begin, size_t code_len, size_t entry_offset) {
    struct ioctl_dom_create_args args = {
        .code_begin = code_begin,
        .code_len = code_len,
        .entry_offset = entry_offset,
        .dom_id = -1
    };
    if (ioctl(dev_fd, IOCTL_DOM_CREATE, (unsigned long)&args)) {
        return -1;
    }
    return args.dom_id;
}

static unsigned long call_dom(dom_id_t dom_id) {
    struct ioctl_dom_call_args args = {
        .dom_id = dom_id,
        .retval = 0
    };
    ioctl(dev_fd, IOCTL_DOM_CALL, (unsigned long)&args);
    return args.retval;
}

int main(int argc, char** argv) {
    int retval = 0;
    if (argc < 2) {
        fprintf(stderr, "Please provide the domain file name!\n");
        return 1;
    }
    char* file_name = argv[1];

    open_device();

    int elf_fd = open(file_name, O_RDONLY);
    if (elf_fd < 0) {
        fprintf(stderr, "Failed to open the file.\n");
        retval = 1;
        goto clean_up_dev;
    }

    struct stat file_stat;
    if (fstat(elf_fd, &file_stat) < 0) {
        fprintf(stderr, "Failed to get state of th file.\n");
        retval = 1;
        goto clean_up_file;
    }

    Elf64_Ehdr *elf_header = (Elf64_Ehdr*)mmap(NULL, file_stat.st_size, PROT_READ,
        MAP_SHARED, elf_fd, 0);
    if (!elf_header) {
        fprintf(stderr, "Failed to set up mmap.\n");
        retval = 1;
        goto clean_up_file;
    }

    if (strncmp(ELF_HEADER_MAGIC, elf_header->e_ident, sizeof(ELF_HEADER_MAGIC)))
    {
        fprintf(stderr, "Not an ELF file.\n");
        retval = 1;
        goto clean_up_mmap;
    }

    if (elf_header->e_machine != EM_RISCV) {
        fprintf(stderr, "Not for RISC-V.\n");
        retval = 1;
        goto clean_up_mmap;
    }

    printf("Ok, good file.\n");
    
    Elf64_Phdr *phdrs = (Elf64_Phdr*)(((void*)elf_header) + elf_header->e_phoff);
    Elf64_Half phnum = elf_header->e_phnum;

    printf("Found %lu segments\n", phnum);

    int ph_idx;
    for (ph_idx = 0; ph_idx < phnum; ph_idx ++) {
        if (phdrs[ph_idx].p_type == PT_LOAD && (phdrs[ph_idx].p_flags & PF_X)) {
            break;
        }
    }
    if (ph_idx >= phnum) {
        fprintf(stderr, "No loadable executable segment found.\n");
        retval = 1;
        goto clean_up_mmap;
    }
    printf("Loadable executable segment found.\n");
    printf("Entry address = %lx\n", elf_header->e_entry);
    printf("Virtual address = %lx\n", phdrs[ph_idx].p_vaddr);
    printf("File offset = %lx\n", phdrs[ph_idx].p_offset);
    printf("Segment size = %lx\n", phdrs[ph_idx].p_filesz);

    if (elf_header->e_entry < phdrs[ph_idx].p_vaddr ||
        elf_header->e_entry >= (phdrs[ph_idx].p_vaddr + phdrs[ph_idx].p_filesz))
    {
        fprintf(stderr, "Entry not within the loaded segment!\n");
        retval = 1;
        goto clean_up_mmap;
    }

    dom_id_t dom_id = create_dom(((void*)elf_header) + phdrs[ph_idx].p_offset, phdrs[ph_idx].p_filesz,
            elf_header->e_entry - phdrs[ph_idx].p_vaddr);
    printf("Created domain ID = %lu\n", dom_id);

    unsigned long dom_retval = call_dom(dom_id);
    printf("Called dom retval = %lu\n", dom_retval);

    // FIXME: repeated calls not working right now
    // dom_retval = call_dom(dom_id);
    // printf("Called dom retval (2nd) = %lu\n", dom_retval);

clean_up_mmap:
    munmap(elf_header, file_stat.st_size);

clean_up_file:
    close(elf_fd);


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
