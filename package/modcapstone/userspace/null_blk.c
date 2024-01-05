#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/libcapstone.h"

#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

int main() {
    capstone_init();

    /* domain id and region id indexing is by assumption for simplicity  */
    dom_id_t dom_id = create_dom("/test-domains/sbi.dom", "/nullb/capstone_split/nullb_split.smode");
    print_nobuf("SBI domain created with ID %lu\n", dom_id);
    region_id_t func_region_id = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", func_region_id);
    region_id_t rv_region_id = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", rv_region_id);
    region_id_t data_region_id = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", data_region_id);

    share_region(dom_id, func_region_id);
    share_region(dom_id, rv_region_id);
    share_region(dom_id, data_region_id);

    capstone_cleanup();

    return 0;
}
