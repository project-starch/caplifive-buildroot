#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))

#define PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))
#define CAPSTONE_DPI_REGION_SHARE 0x1

#define SERVER_GET 0xacef
#define SERVER_PUT 0xaddd
#define CLIENT_GET 0xcef
#define CLIENT_PUT 0xcadd
#define ACK 0xacc

static unsigned *shared_region;

__domentry __domreentry void mem(__domret void* ra, unsigned func, unsigned* res) {
    if (func == CAPSTONE_DPI_REGION_SHARE) {
        shared_region = res;
    } else {
        unsigned op = *shared_region;
        op = op << 32 >> 32;

        if (op == CLIENT_GET) {
            /**
             * The client got something from the server
            */

            /**
             * Clear the operation
            */
        //    *shared_region = 0;

            /**
            * Print data from the server domain for now
            */
            unsigned vars_nr = *(shared_region + 4);
            // *(shared_region + 4) = 0;
            vars_nr = vars_nr << 32 >> 32;
            PRINT(vars_nr);
            unsigned i = 0;
            while (i < vars_nr) {
                PRINT(*(shared_region + 8 + 4 * i) << 32 >> 32);
                // *(shared_region + 8 + 4 * i) = 0;
                i = i + 1;
            }

            // *shared_region = ACK; /** TODO: Why is this not working and causing OOB access */
            *res = ACK;
            __domreturn(ra, __mem_reentry, 0);
        }
        if (op == SERVER_GET) {
            /**
             * The client should put something in the shared region for the serve to get
            */
           PRINT(0xdeadbeef);
           op = 123;
           *res = ACK;
           __domreturn(ra, __mem_reentry, 0);
        }
    }

    __domreturn(ra, __mem_reentry, 0);
}
