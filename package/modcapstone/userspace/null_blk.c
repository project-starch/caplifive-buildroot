#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/libcapstone.h"

int main() {
    capstone_init();

    /* FIXME: domain id and region ids are not synced with nullb, nullb just assume the ids are from 0 for now */
    dom_id_t dom_id = create_dom("/test-domains/sbi.dom", "/nullb/capstone_split/nullb_split.smode");
    region_id_t func_region_id = create_region(4096);
    region_id_t rv_region_id = create_region(4096);
    region_id_t data_region_id = create_region(4096);

    share_region(dom_id, func_region_id);
    share_region(dom_id, rv_region_id);
    share_region(dom_id, data_region_id);

    capstone_cleanup();

    return 0;
}
