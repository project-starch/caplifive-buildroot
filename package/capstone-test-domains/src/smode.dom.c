/**
 * This domain multiplexes multiple functions on a single entry point
 * with a function code.
 * The initial-S function initialises the S mode using code provided through a
 * capability that is passed in as an argument.
 * 
*/


#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))
#define __domreentryrestores __attribute__((domreentryrestores))

#define PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

#define FUNC_INIT 0x0
#define FUNC_CALL 0x1

unsigned initialised;

__attribute__((naked)) static void trap_entry() {
    __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, a0, x0");
    __asm__ volatile("ccsrrw(sp, cscratch, sp)");
    __asm__ volatile("stc(t0, sp, -16)");
    __asm__ volatile("ccsrrw(t0, cepc, x0)");
    __asm__ volatile("addi t0, t0, 4");
    __asm__ volatile("ccsrrw(x0, cepc, t0)");
    __asm__ volatile("ldc(t0, sp, -16)");
    __asm__ volatile("ccsrrw(sp, cscratch, sp)");
    __asm__ volatile("mret");
}

__domentry __domreentryrestores void call_handler(__domret void* ra, unsigned *buf) {
    if (initialised) {
        *buf = 1;
    } else {
        // initialise the S mode with code in buf
        // FIXME: set cmmu when it is fixed
        // assuming the entry is right at the beginning for now
        __asm__ volatile ("ccsrrw(x0,cepc,%0)" :: "r"(buf));
        __asm__ volatile ("csrs mstatus, %0" :: "r"(1 << 11));
        __asm__ volatile ("csrw satp, x0");
        __asm__ volatile ("ccsrrw(x0, ctvec, %0)" :: "r"(trap_entry));
        __asm__ volatile ("csrw medeleg, x0");
        __asm__ volatile ("ccsrrw(x0, cscratch, sp)");
        // PRINT(0x42);
        // __asm__ volatile ("csrs mideleg, ")
        // FIXME: set mtvec
        __asm__ volatile ("mret");

        initialised = 1;
    }

    __domreturnsaves(ra, __call_handler_reentry, 0);
}
