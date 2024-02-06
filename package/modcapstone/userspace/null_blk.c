#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/libcapstone.h"

#define DOMAIN_NULLB_SPLIT 0x0
#define PRIME_REGION_ID 11

#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

int main() {
    capstone_init();

    dom_id_t dom_id = create_dom_ko("/test-domains/sbi.dom", "/nullb/capstone_split/nullb_split.smode.ko");
    print_nobuf("SBI domain created with ID %lu\n", dom_id);
    assert(dom_id == DOMAIN_NULLB_SPLIT);
    /* the first region serves as 2 roles: (we call it the prime region)
     * 1. the function code region, shared by the current domain and the new domain
     * 2. the id sync region, shared by null_blk.user and null_blk.ko
    */
    region_id_t func_region_id = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", func_region_id);
    region_id_t rv_region_id = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", rv_region_id);
    region_id_t data_region_id = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", data_region_id);
    assert(func_region_id == PRIME_REGION_ID);

    /* map the prime region to share ids with ko*/
    unsigned long *region_base = map_region(func_region_id, 4096);
    memcpy(region_base, &rv_region_id, sizeof(rv_region_id));
    memcpy(region_base + 1, &data_region_id, sizeof(data_region_id));
    print_nobuf("region_base[0]: %lu\n", region_base[0]);
    print_nobuf("region_base[1]: %lu\n", region_base[1]);

    share_region(dom_id, func_region_id);
    share_region(dom_id, rv_region_id);
    share_region(dom_id, data_region_id);

    capstone_cleanup();

    return 0;
}
