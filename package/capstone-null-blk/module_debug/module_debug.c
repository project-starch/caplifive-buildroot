#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

static char stack[4096];

static void module_main(void) {
    while (1) {
        __asm__ volatile ("add x0, x0, x0");
        __asm__ volatile ("add x0, x0, x0");
        __asm__ volatile ("add x0, x0, x0");
        __asm__ volatile ("add x0, x0, x0");
        __asm__ volatile ("add x0, x0, x0");
        __asm__ volatile ("add x0, x0, x0");
    }
}

// __attribute__((naked)) void module_start(void) {
//     __asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
// 	// __asm__ volatile ("j module_main");
//     module_main();
// }

static __attribute__((naked)) int __init module_debug_init(void)
{
	// module_start();
    __asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
    module_main();

    // while (1) {
    //     __asm__ volatile ("add x0, x0, x0");
    //     __asm__ volatile ("add x0, x0, x0");
    //     __asm__ volatile ("add x0, x0, x0");
    //     __asm__ volatile ("add x0, x0, x0");
    //     __asm__ volatile ("add x0, x0, x0");
    //     __asm__ volatile ("add x0, x0, x0");
    // }

	return 0;
}

static void __exit module_debug_exit(void)
{
	return;
}

module_init(module_debug_init);
module_exit(module_debug_exit);

MODULE_LICENSE("GPL");
