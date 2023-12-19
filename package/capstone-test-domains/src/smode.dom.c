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

#define PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

#define FUNC_INIT 0x0
#define FUNC_CALL 0x1

unsigned initialised;

__domentry __domreentry void call_handler(__domret void* ra, unsigned *buf) {
    if (initialised) {
        *buf = 1;
    } else {
        // initialise the S mode with code in buf
        // FIXME: set cmmu when it is fixed
        // assuming the entry is right at the beginning for now
        unsigned entry_addr;
        entry_addr = __capfield(buf, 2);
        PRINT(buf);
        PRINT(entry_addr);
        __asm__ volatile ("csrw mepc, %0" :: "r"(entry_addr));
        __asm__ volatile ("csrs mstatus, %0" :: "r"(1 << 11));
        __asm__ volatile ("csrw satp, x0");
        // __asm__ volatile ("csrs mideleg, ")
        // FIXME: set mtvec
        __asm__ volatile ("mret");

        initialised = 1;
    }

    __domreturn(ra, __call_handler_reentry, 0);
}
