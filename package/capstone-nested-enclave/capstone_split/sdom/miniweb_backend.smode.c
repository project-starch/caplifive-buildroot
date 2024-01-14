#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include "miniweb_backend.smode.h"

static char stack[4096];

static void main(void) {
    while (1) {
        unsigned long rv = 0;

        sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_RETURN,
			rv, 0, 0, 0, 0, 0);
    }
}

static __attribute__((naked)) int __init miniweb_backend_init(void)
{
	__asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
	main();

	return 0;
}

static void __exit miniweb_backend_exit(void)
{
	return;
}

module_init(miniweb_backend_init);
module_exit(miniweb_backend_exit);

MODULE_LICENSE("GPL");
