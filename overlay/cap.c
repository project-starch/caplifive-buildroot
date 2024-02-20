#include<stdio.h>

#define GENCAP(d, base, end) asm volatile(".insn r 0x5b, 0x1, 0x40, %0, %1, %2" : "=r"(d) : "r"(base), "r"(end))

int v;

int main(void) {
    int good;

    asm volatile (
        ".insn r 0x5b, 0x1, 0x40, a0, %1, %2;\r\n"
        ".insn r 0x5b, 0x1, 0x43, x0, a0, x0;\r\n"
        "li s1, 42;\r\n"
        "sd s1, 0(a0);\r\n"
        "ld s1, 0(a0);\r\n"
        "mv %0, s1"
        : "=r"(good) : "r"(&v), "r"(&v + sizeof(v))
        : "a0", "a1", "memory"
    );

    printf("%d\n", good);

    return 0;
}
