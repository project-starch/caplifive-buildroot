#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))

#define PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" ::"r"(v))
#define CAPSTONE_DPI_REGION_SHARE 0x1

#define SERVER_GET 0xacef
#define SERVER_PUT 0xaddd
#define CLIENT_GET 0xccef
#define CLIENT_PUT 0xcadd
#define ACK 0xaccc

#define PROD_NUMBER 25

static unsigned *g_shared_region;

__domentry __domreentry void dpdk_client1(__domret void *ra, unsigned action, unsigned *res)
{
    unsigned i = 0;

    if (action == CAPSTONE_DPI_REGION_SHARE) {
        g_shared_region = res;
    } else {
        unsigned op = g_shared_region[0];
        // unsigned op = *g_shared_region;
        // op = op << 32 >> 32;

        if (op == CLIENT_PUT) {
            /**
             * The client got something from the server
             */

            /**
             * Print data from the server domain for now
             */
            // unsigned vars_nr = *(g_shared_region + 4);
            unsigned vars_nr = g_shared_region[1];
            g_shared_region[1] = 0;
            // *(g_shared_region + 4) = 0;
            // vars_nr = vars_nr << 32 >> 32;

            PRINT(vars_nr);

            while (i < vars_nr) {
                // unsigned val = *(g_shared_region + 8 + 4 * i);
                // val = val << 32 >> 32;
                unsigned val = g_shared_region[i + 2];
                PRINT(val);

                i = i + 1;
            }

            i = 0;
            while (i < vars_nr) {
                // *(g_shared_region + 8 + 4 * i) = 0;
                g_shared_region[i + 2] = 0;
                i = i + 1;
            }

            *g_shared_region = ACK;
            // g_shared_region[0] = ACK;
            /**
             * Return the number of consumed values so the serve knows if more domains calls are needed
            */
            *res = vars_nr;
            __domreturn(ra, __dpdk_client1_reentry, 0);
        }
        if (op == SERVER_GET) {
            /**
             * The client should produce something for the serve to consume
             */

            /**
             * Produce values for the server process
            */
        //    *g_shared_region = SERVER_PUT;  /** Set the proper operation for the server - disabled for now */
        //    *(g_shared_region + 4) = PROD_NUMBER;

            g_shared_region[1] = PROD_NUMBER;

            i = 0;
            while (i < PROD_NUMBER) {
                // *(g_shared_region + 8 + 4 * i) = i * i * i * i;

                g_shared_region[i + 2] = i * i * i * i;
                i = i + 1;
            }

            // *g_shared_region = ACK;
            g_shared_region[0] = ACK;
            *res = PROD_NUMBER;
            __domreturn(ra, __dpdk_client1_reentry, 0);
        }
    }

    __domreturn(ra, __dpdk_client1_reentry, 0);
}
