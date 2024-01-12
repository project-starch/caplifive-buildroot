#define SBI_EXT_CAPSTONE 0x12345678
#define SBI_EXT_CAPSTONE_DOM_CREATE 0x0
#define SBI_EXT_CAPSTONE_DOM_CALL   0x1
#define SBI_EXT_CAPSTONE_DOM_CALL_WITH_CAP   0x2
#define SBI_EXT_CAPSTONE_REGION_CREATE   0x3
#define SBI_EXT_CAPSTONE_REGION_SHARE    0x4
#define SBI_EXT_CAPSTONE_DOM_RETURN      0x5 


typedef unsigned long uintptr_t;

static char stack[4096];

static int fibonacci(int n) {
    int a = 1, b = 1, tmp;
    while (n --) {
        tmp = a + b;
        a = b;
        b = tmp;
    }

    return b;
}

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

static void main() {
	while(1) {
		int r = fibonacci(10);
		struct sbiret _ = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_RETURN,
			r, 0, 0, 0, 0, 0);
	}
}

__attribute__((naked)) void _start() {
    __asm__ volatile ("mv sp, %0" :: "r"(stack + 4096)); // setup stack
	__asm__ volatile ("j main");
	while(1); // should not reach here
}
