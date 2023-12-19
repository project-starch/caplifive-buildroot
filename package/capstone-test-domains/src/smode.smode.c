
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

__attribute__((naked)) void _start() {
    __asm__ volatile ("mv sp, %0" :: "r"(stack + 4096)); // setup stack
    int r = fibonacci(10);
	register unsigned long a0 asm ("a0") = (unsigned long)(r);
    __asm__ volatile ("ecall" :: "r"(a0));
    while(1);
}
