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

    capstone_cleanup();

    return 0;
}
