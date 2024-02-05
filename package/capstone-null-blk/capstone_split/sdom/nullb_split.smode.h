#ifndef __NULLB_SPLIT_SMODE_H
#define __NULLB_SPLIT_SMODE_H

/*domain*/
#define NULLBS_NULL_VALIDATE_CONF 0x0
#define NULLBS_NULLB_TO_QUEUE 0x1
#define NULLBS_BIO_OP 0x2
#define NULLBS_END_CMD_BIO 0x3

typedef unsigned long dom_id_t;
typedef unsigned long region_id_t;

/*null block driver*/
#include "../module/null_blk.h"

static int g_poll_queues = 1;
// set nr_cpu_ids to 1 (as per qemu impl) to avoid undefined extern symbol
static unsigned int qemu_nr_cpu_ids = 1;

enum {
	NULL_IRQ_NONE		= 0,
	NULL_IRQ_SOFTIRQ	= 1,
	NULL_IRQ_TIMER		= 2,
};

enum nullb_device_flags {
	NULLB_DEV_FL_CONFIGURED	= 0,
	NULLB_DEV_FL_UP		= 1,
	NULLB_DEV_FL_THROTTLED	= 2,
	NULLB_DEV_FL_CACHE	= 3,
};

/*sbi*/
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

#endif /* __NULLB_SPLIT_SMODE_H */
