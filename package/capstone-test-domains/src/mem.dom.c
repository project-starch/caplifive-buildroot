#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))

#define PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

static unsigned ok = 0;
static unsigned counter = 0;
static unsigned is_server = 0;

__domentry __domreentry void mem(__domret void* ra, unsigned func, unsigned* res) {
    if (is_server) {
        *res = 123;
        is_server = 0;
    } else {
        *res = 1234;
        is_server = 1;
    }

    *res = 123;
    __domreturn(ra, __mem_reentry, 0);
}
