#ifndef __MINIWEB_BACKEND_SMODE_H
#define __MINIWEB_BACKEND_SMODE_H

typedef unsigned long dom_id_t;
typedef unsigned long region_id_t;

/* html_fd_status */
#define HTML_FD_UNDEFINED 0
#define HTML_FD_404RESPONSE 1
#define HTML_FD_200RESPONSE 2
#define HTML_FD_CGI 3

#define C_DOMAIN_DATA_SIZE 4096 * 2

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

#define CAPSTONE_REGION_FIELD_BASE    0x0
#define CAPSTONE_REGION_FIELD_END     0x1
#define CAPSTONE_REGION_FIELD_LEN     0x2

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
