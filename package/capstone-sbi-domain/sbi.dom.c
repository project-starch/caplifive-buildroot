#include "capstone-sbi/sbi_capstone.c"

#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))
#define __domreentryrestores __attribute__((domreentryrestores))

unsigned initialised;

__domentry __domreentryrestores void entry(__domret void *ra, unsigned func, unsigned *buf) {
    if (initialised) {
        int handled = handle_dpi(func, buf);
        if (!handled) {
            __asm__ volatile ("ccsrrw(x0, cscratch, sp)");
            __asm__ volatile ("mret");
        }
    } else {
        __asm__ volatile ("ccsrrw(x0,cepc,%0)" :: "r"(buf));
        __asm__ volatile ("csrs mstatus, %0" :: "r"(1 << 11));
        __asm__ volatile ("csrw satp, x0");
        __asm__ volatile ("ccsrrw(x0, ctvec, %0)" :: "r"(_cap_trap_entry));
        __asm__ volatile ("csrw medeleg, x0");

        initialised = 1;
    }

    __domreturnsaves(ra, __entry_reentry, 0);
}
