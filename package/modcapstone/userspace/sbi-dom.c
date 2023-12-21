#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/libcapstone.h"

int main() {
    capstone_init();

    dom_id_t dom_id = create_dom("/test-domains/sbi.dom", "/test-domains/smode.smode");
    region_id_t region_id = create_region(4096);
    char *region_base = map_region(region_id, 4096);
    region_base[0] = 42;
    assert(region_base[0] == 42);
    share_region(dom_id, region_id);

    capstone_cleanup();

    return 0;
}
