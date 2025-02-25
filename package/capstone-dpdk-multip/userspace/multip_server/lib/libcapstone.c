#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <fcntl.h>
#include "libcapstone.h"

#define MAX_REGION_N 64
#define MAP_SIZE_LIMIT 0x10000000
#define DEBUG_COUNTER_SWITCH_U 0
#define debug_counter_inc(counter_no, delta) __asm__ volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1" :: "r"(counter_no), "r"(delta))
#define debug_counter_tick(counter_no) debug_counter_inc((counter_no), 1)

struct ElfCode {
    int fd;
    void *map_base;
    unsigned long code_start, code_len;
    unsigned long loadable_size;
    off_t size, entry_offset;
};

static int dev_fd;
static size_t region_mmap_offsets[MAX_REGION_N];
static int region_mmappable[MAX_REGION_N];
static int region_n;

static const unsigned char ELF_HEADER_MAGIC[4] = {
    ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3
};

static int open_device() {
    dev_fd = open(CAPSTONE_DEV_PATH, O_NONBLOCK | O_RDWR);
    if (dev_fd < 0) {
        return dev_fd;
    }
    return 0;
}

static int close_device() {
    return close(dev_fd);
}

int capstone_init() {
    return open_device();
}

int capstone_cleanup() {
    return close_device();
}

static int load_elf_code(const char *file_name, struct ElfCode *res) {
    int retval = 0;

    int elf_fd = open(file_name, O_RDONLY);
    if (elf_fd < 0) {
        fprintf(stderr, "Failed to open the file.\n");
        return 1;
    }

    struct stat file_stat;
    if (fstat(elf_fd, &file_stat) < 0) {
        fprintf(stderr, "Failed to get state of the file.\n");
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

    res->fd = elf_fd;
    res->map_base = (void*)elf_header;
    res->size = file_stat.st_size;
    res->code_start = (unsigned long)elf_header + phdrs[ph_idx].p_offset;
    res->code_len = phdrs[ph_idx].p_filesz;
    res->entry_offset = elf_header->e_entry - phdrs[ph_idx].p_vaddr;

    unsigned long loadable_start, loadable_end;
    loadable_start = phdrs[ph_idx].p_vaddr;

    for (ph_idx = phnum - 1; ph_idx >= 0; ph_idx --) {
        if (phdrs[ph_idx].p_type == PT_LOAD) {
            break;
        }
    }
    assert(ph_idx >= 0);
    loadable_end = phdrs[ph_idx].p_vaddr + phdrs[ph_idx].p_memsz;
    assert(loadable_end > loadable_start);
    res->loadable_size = loadable_end - loadable_start;

    printf("Loadable size = %lu\n", res->loadable_size);

    return 0;

clean_up_mmap:
    munmap(elf_header, file_stat.st_size);

clean_up_file:
    close(elf_fd);

    return retval;
}

static int load_elf_code_ko(const char *file_name, struct ElfCode *res) {
    int retval = 0;

    int elf_fd = open(file_name, O_RDONLY);
    if (elf_fd < 0) {
        fprintf(stderr, "Failed to open the file.\n");
        return 1;
    }

    struct stat file_stat;
    if (fstat(elf_fd, &file_stat) < 0) {
        fprintf(stderr, "Failed to get state of the file.\n");
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

    Elf64_Shdr *shdrs = (Elf64_Shdr*)(((void*)elf_header) + elf_header->e_shoff);
    Elf64_Half shnum = elf_header->e_shnum;
    
    printf("Found %lu section headers\n", shnum);

    int sh_idx;
    char* shstrtab = (char*)(((void*)elf_header) + shdrs[elf_header->e_shstrndx].sh_offset);

    int init_text_sh_idx = -1;
    int exec_sh_idx = -1;

    for (sh_idx = 0; sh_idx < shnum; sh_idx ++) {
        if (shdrs[sh_idx].sh_type == SHT_PROGBITS && shdrs[sh_idx].sh_flags == (SHF_ALLOC | SHF_EXECINSTR)) {
            if (exec_sh_idx == -1) {
                exec_sh_idx = sh_idx;
                printf("Found executable section header.\n");
            }
            
            if (strcmp(shstrtab + shdrs[sh_idx].sh_name, ".init.text") == 0) {
                init_text_sh_idx = sh_idx;
                printf(".init.text found.\n");
            }

            if (init_text_sh_idx != -1 && exec_sh_idx != -1) {
                break;
            }
        }
    }
    if (sh_idx >= shnum) {
        fprintf(stderr, ".init.text not found.\n");
        retval = 1;
        goto clean_up_mmap;
    }

    res->fd = elf_fd;
    res->map_base = (void*)elf_header;
    res->size = file_stat.st_size;
    unsigned long exec_start = (unsigned long)elf_header + shdrs[exec_sh_idx].sh_addr + shdrs[exec_sh_idx].sh_offset;
    unsigned long init_text_start = (unsigned long)elf_header + shdrs[init_text_sh_idx].sh_addr + shdrs[init_text_sh_idx].sh_offset;
    res->code_start = exec_start;
    printf("Code start = %lx\n", res->code_start);
    unsigned long init_text_len = shdrs[init_text_sh_idx].sh_size;
    res->code_len = init_text_start + init_text_len - exec_start;
    printf("Code len = %lx\n", res->code_len);
    res->entry_offset = init_text_start - exec_start;

    unsigned long loadable_start, loadable_end;
    loadable_start = shdrs[exec_sh_idx].sh_addr + shdrs[exec_sh_idx].sh_offset;

    for (sh_idx = shnum - 1; sh_idx >= 0; sh_idx --) {
        if (shdrs[sh_idx].sh_type == SHT_PROGBITS && shdrs[sh_idx].sh_flags == (SHF_ALLOC | SHF_EXECINSTR)) {
            break;
        }
    }

    assert(sh_idx >= 0);
    loadable_end = shdrs[sh_idx].sh_addr + shdrs[sh_idx].sh_offset + shdrs[sh_idx].sh_size;
    assert(loadable_end > loadable_start);
    res->loadable_size = loadable_end - loadable_start;
    printf("Loadable size = %lu\n", res->loadable_size);

    return 0;

clean_up_mmap:
    munmap(elf_header, file_stat.st_size);

clean_up_file:
    close(elf_fd);

    return retval;
}

static void release_elf_code(struct ElfCode *elf_code) {
    munmap(elf_code->map_base, elf_code->code_len);
    close(elf_code->fd);
}

static dom_id_t create_dom_from_elf(const struct ElfCode *c_code,
                           const struct ElfCode *s_code) {
    struct ioctl_dom_create_args args = {
        .code_begin = c_code->code_start,
        .code_len = c_code->code_len,
        .entry_offset = c_code->entry_offset,
        .dom_id = -1
    };
    
    if(s_code) {
        args.s_load_begin = s_code->code_start;
        args.s_load_len = s_code->code_len;
        args.s_entry_offset = s_code->entry_offset;
        args.s_size = s_code->loadable_size;
    } else {
        args.s_load_len = 0;
    }

    if (ioctl(dev_fd, IOCTL_DOM_CREATE, (unsigned long)&args)) {
        return -1;
    }
    return args.dom_id;
}

dom_id_t create_dom(const char *c_path, const char *s_path) {
    if(!c_path) {
        return -1;
    }
    struct ElfCode c_code;
    
    dom_id_t res = -1;
    int retval = load_elf_code(c_path, &c_code);
    if(retval)
        return retval;
    
    if(s_path) {
        struct ElfCode s_code;
        retval = load_elf_code(s_path, &s_code);
        if(retval)
            goto c_code_cleanup;
        res = create_dom_from_elf(&c_code, &s_code);

        release_elf_code(&s_code);
    } else {
        res = create_dom_from_elf(&c_code, NULL);
    }

c_code_cleanup:
    release_elf_code(&c_code);

    return res;
}

dom_id_t create_dom_ko(const char *c_path, const char *s_path) {
    if(!c_path) {
        return -1;
    }
    struct ElfCode c_code;
    
    dom_id_t res = -1;
    int retval = load_elf_code(c_path, &c_code);
    if(retval)
        return retval;
    
    if(s_path) {
        struct ElfCode s_code;
        retval = load_elf_code_ko(s_path, &s_code);
        if(retval)
            goto c_code_cleanup;
        res = create_dom_from_elf(&c_code, &s_code);

        release_elf_code(&s_code);
    } else {
        res = create_dom_from_elf(&c_code, NULL);
    }

c_code_cleanup:
    release_elf_code(&c_code);

    return res;
}

unsigned long call_dom(dom_id_t dom_id) {
    struct ioctl_dom_call_args args = {
        .dom_id = dom_id,
        .retval = 0
    };
    debug_counter_tick(DEBUG_COUNTER_SWITCH_U);
    ioctl(dev_fd, IOCTL_DOM_CALL, (unsigned long)&args);
    return args.retval;
}

region_id_t create_region(unsigned long len) {
    struct ioctl_region_create_args args = {
        .len = len,
        .region_id = -1
    };
    ioctl(dev_fd, IOCTL_REGION_CREATE, (unsigned long)&args);
    return args.region_id;
}

void shared_region_annotated(dom_id_t dom_id, region_id_t region_id, unsigned long annotation_perm, unsigned long annotation_rev) {
    struct ioctl_region_share_annotated_args args = {
        .dom_id = dom_id,
        .region_id = region_id,
        .annotation_perm = annotation_perm,
        .annotation_rev = annotation_rev,
        .retval = 0
    };
    debug_counter_tick(DEBUG_COUNTER_SWITCH_U);
    ioctl(dev_fd, IOCTL_REGION_SHARE_ANNOTATED, (unsigned long)&args);
}

void share_region(dom_id_t dom_id, region_id_t region_id) {
    struct ioctl_region_share_args args = {
        .dom_id = dom_id,
        .region_id = region_id,
        .retval = 0
    };
    debug_counter_tick(DEBUG_COUNTER_SWITCH_U);
    ioctl(dev_fd, IOCTL_REGION_SHARE, (unsigned long)&args);
}

void revoke_region(region_id_t region_id) {
    struct ioctl_region_revoke_args args = {
        .region_id = region_id,
        .retval = 0
    };
    ioctl(dev_fd, IOCTL_REGION_REVOKE, (unsigned long)&args);
}

void *map_region(region_id_t region_id, unsigned long len) {
    while(region_n <= region_id) {
        struct ioctl_region_query_args region_query_args;
        region_query_args.region_id = region_n;
        ioctl(dev_fd, IOCTL_REGION_QUERY, (unsigned long)&region_query_args);
        if(region_query_args.len == 0) {
            return NULL;
        }
        region_mmap_offsets[region_n] = region_query_args.mmap_offset;
        region_mmappable[region_n] = region_query_args.len < MAP_SIZE_LIMIT;
        ++ region_n;
    }
    if(!region_mmappable[region_id]) {
        return NULL;
    }
    return mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
        dev_fd, region_mmap_offsets[region_id]);
}

void probe_regions(void) {
    ioctl(dev_fd, IOCTL_REGION_PROBE, 0);
}

int region_count(void) {
    return region_n;
}

void schedule_dom(dom_id_t dom_id) {
    struct ioctl_dom_sched_args args;
    args.dom_id = dom_id;
    ioctl(dev_fd, IOCTL_DOM_SCHEDULE, (unsigned long)&args);
}
