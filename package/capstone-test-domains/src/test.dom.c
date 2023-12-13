#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))

#define PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

__domentry void test(__domret void* ra, unsigned* res) {
    // while(1);
    unsigned a = 1;
    unsigned b = 1;
    unsigned tmp;

    int i;
    for(i = 1; i < 10; i += 1) {
        tmp = a + b;
        a = b;
        b = tmp;
    }

    *res = b;
    __domreturn(ra, test, 0);
}
