#include "nullb_split.smode.h"

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

static void main() {
	while(1) {
		int r = fibonacci(10);
		struct sbiret _ = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_RETURN,
			r, 0, 0, 0, 0, 0);
	}
}

__attribute__((naked)) void _start() {
    __asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
	__asm__ volatile ("j main");
}
