#ifndef __MINIWEB_BACKEND_SMODE_H
#define __MINIWEB_BACKEND_SMODE_H

typedef unsigned long dom_id_t;
typedef unsigned long region_id_t;

/* elf */
#define EM_RISCV	243
#define EI_NIDENT	16

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7 /* Thread local storage segment */

#define PF_R		0x4
#define PF_W		0x2
#define PF_X		0x1

typedef unsigned long long	Elf64_Addr;
typedef unsigned short	Elf64_Half;
typedef short	Elf64_SHalf;
typedef unsigned long long	Elf64_Off;
typedef int	Elf64_Sword;
typedef unsigned int	Elf64_Word;
typedef unsigned long long	Elf64_Xword;
typedef long long	Elf64_Sxword;

typedef struct elf64_hdr {
  unsigned char	e_ident[EI_NIDENT];	/* ELF "magic number" */
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;		/* Entry point virtual address */
  Elf64_Off e_phoff;		/* Program header table file offset */
  Elf64_Off e_shoff;		/* Section header table file offset */
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct elf64_phdr {
  Elf64_Word p_type;
  Elf64_Word p_flags;
  Elf64_Off p_offset;		/* Segment file offset */
  Elf64_Addr p_vaddr;		/* Segment virtual address */
  Elf64_Addr p_paddr;		/* Segment physical address */
  Elf64_Xword p_filesz;		/* Segment size in file */
  Elf64_Xword p_memsz;		/* Segment size in memory */
  Elf64_Xword p_align;		/* Segment alignment, file & memory */
} Elf64_Phdr;

/* html_fd_status */
#define HTML_FD_UNDEFINED 0
#define HTML_FD_404RESPONSE 1
#define HTML_FD_200RESPONSE 2
#define HTML_FD_CGI 3

#define C_DOMAIN_DATA_SIZE (4096 * 2)

/* sbi */
#define SBI_EXT_CAPSTONE 0x12345678
#define SBI_EXT_CAPSTONE_DOM_CREATE 0x0
#define SBI_EXT_CAPSTONE_DOM_CALL   0x1
#define SBI_EXT_CAPSTONE_DOM_CALL_WITH_CAP   0x2
#define SBI_EXT_CAPSTONE_REGION_CREATE   0x3
#define SBI_EXT_CAPSTONE_REGION_SHARE    0x4
#define SBI_EXT_CAPSTONE_DOM_RETURN      0x5
#define SBI_EXT_CAPSTONE_REGION_QUERY    0x6
#define SBI_EXT_CAPSTONE_DOM_SCHEDULE    0x7
#define SBI_EXT_CAPSTONE_REGION_COUNT    0x8
#define SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED    0x9
#define SBI_EXT_CAPSTONE_REGION_REVOKE    0xa
#define SBI_EXT_CAPSTONE_REGION_DE_LINEAR    0xb
#define SBI_EXT_CAPSTONE_REGION_POP   0xc

#define CAPSTONE_REGION_FIELD_BASE    0x0
#define CAPSTONE_REGION_FIELD_END     0x1
#define CAPSTONE_REGION_FIELD_LEN     0x2

#define CAPSTONE_ANNOTATION_PERM_IN 0x0
#define CAPSTONE_ANNOTATION_PERM_INOUT 0x1
#define CAPSTONE_ANNOTATION_PERM_OUT 0x2
#define CAPSTONE_ANNOTATION_PERM_EXE 0x3
#define CAPSTONE_ANNOTATION_PERM_FULL 0x4

#define CAPSTONE_ANNOTATION_REV_DEFAULT 0x0
#define CAPSTONE_ANNOTATION_REV_BORROWED 0x1
#define CAPSTONE_ANNOTATION_REV_SHARED 0x2
#define CAPSTONE_ANNOTATION_REV_TRANSFERRED 0x3

#define METADATA_STATUS_OFFSET 0
#define METADATA_HTML_LEN_OFFSET sizeof(unsigned long)
#define METADATA_SOCKET_LEN_OFFSET (2 * sizeof(unsigned long))
#define METADATA_HTML_PATH_OFFSET (2 * sizeof(unsigned long) + sizeof(unsigned long long))

typedef unsigned long uintptr_t;

struct sbiret {
	long error;
	long value;
};

static struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5)
{
	struct sbiret ret;

	register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);
	register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);
	register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);
	register uintptr_t a3 asm ("a3") = (uintptr_t)(arg3);
	register uintptr_t a4 asm ("a4") = (uintptr_t)(arg4);
	register uintptr_t a5 asm ("a5") = (uintptr_t)(arg5);
	register uintptr_t a6 asm ("a6") = (uintptr_t)(fid);
	register uintptr_t a7 asm ("a7") = (uintptr_t)(ext);
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	ret.error = a0;
	ret.value = a1;

	return ret;
}

#endif // __MINIWEB_BACKEND_SMODE_H
