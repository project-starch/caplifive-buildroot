#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))

#define C_PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

#define CAPSTONE_DPI_CALL             0x0
#define CAPSTONE_DPI_REGION_SHARE     0x1

#define ACK 0x0
#define SERVER_SEND 0x1
#define SERVER_RECEIVE 0x2

#define MAX_REGION_N 4
#define REGION_POP_NUM 1

#define SIZE_OF_ULL 8

#define PROD_NUMBER 40

#define DEBUG_COUNTER_SWITCH_C  2
#define DEBUG_COUNTER_SHARED 10
#define DEBUG_COUNTER_SHARED_TIMES 11
#define DEBUG_COUNTER_BORROWED 12
#define DEBUG_COUNTER_BORROWED_TIMES 13
#define DEBUG_COUNTER_DOUBLE_TRANSFERRED 14
#define DEBUG_COUNTER_DOUBLE_TRANSFERRED_TIMES 15
#define DEBUG_COUNTER_BORROWED_TRANSFERRED 16
#define DEBUG_COUNTER_BORROWED_TRANSFERRED_TIMES 17
#define DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED 18
#define DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED_TIMES 19
#define debug_counter_inc(counter_no, delta) __asm__ volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1" :: "r"(counter_no), "r"(delta))
#define debug_counter_tick(counter_no) debug_counter_inc((counter_no), 1)
#define debug_shared_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_SHARED, delta); debug_counter_inc(DEBUG_COUNTER_SHARED_TIMES, 1)
#define debug_borrowed_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_BORROWED, delta); debug_counter_inc(DEBUG_COUNTER_BORROWED_TIMES, 1)
#define debug_double_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_DOUBLE_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_DOUBLE_TRANSFERRED_TIMES, 1)
#define debug_borrowed_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_BORROWED_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_BORROWED_TRANSFERRED_TIMES, 1)
#define debug_mutable_borrowed_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED_TIMES, 1)

void* regions[MAX_REGION_N];
unsigned region_n = 0;

void dpdk_client(void)
{
    unsigned i = 0;

    void* metadata_region = regions[0];
    unsigned* shared_region = (unsigned *)metadata_region;

    unsigned op = shared_region[0];

    if (op == SERVER_SEND) {
        unsigned vars_nr = shared_region[1];
        void* send_region = regions[1];
        unsigned* send_region_ptr = (unsigned *)send_region;

        while (i < vars_nr) {
            unsigned val = send_region_ptr[i];
            i = i + 1;
        }

        shared_region[0] = ACK;
        debug_shared_counter_inc(SIZE_OF_ULL);
    }
    if (op == SERVER_RECEIVE) {
        void* receive_region = regions[1];
        unsigned* receive_region_ptr = (unsigned *)receive_region;

        i = 0;
        while (i < PROD_NUMBER) {
            receive_region_ptr[i] = i + i + i + i;
            i = i + 1;
        }
        debug_borrowed_counter_inc(i * SIZE_OF_ULL);

        shared_region[0] = ACK;
        shared_region[1] = PROD_NUMBER;
        debug_shared_counter_inc(2 * SIZE_OF_ULL);
    }
}

void dpi_call(void) {
    dpdk_client();
    region_n -= REGION_POP_NUM;
}

void dpi_share_region(void *region) {
    regions[region_n] = region;
    region_n += 1;
}

unsigned handle_dpi(unsigned func, void *arg) {
    unsigned handled = 0;

    switch(func) {
        case CAPSTONE_DPI_CALL:
            dpi_call();
            handled = 1;
            break;
        case CAPSTONE_DPI_REGION_SHARE:
            dpi_share_region(arg);
            handled = 1;
            break;
    }

    return handled;
}

__domentry __domreentry void dpdk_multip_client_0_entry(__domret void *ra, unsigned func, unsigned *buf) {
    __domret void *caller_dom = ra;
    
    unsigned handled = handle_dpi(func, buf);
    if(!handled) {
        C_PRINT(0xdeadbeef);
        while(1);
    }
    ra = caller_dom;

    debug_counter_tick(DEBUG_COUNTER_SWITCH_C);
    __domreturn(ra, __dpdk_multip_client_0_entry_reentry, 0);
}
