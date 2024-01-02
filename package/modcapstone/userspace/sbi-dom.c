#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/libcapstone.h"

#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

int main() {
    capstone_init();

    dom_id_t dom_id = create_dom("/test-domains/sbi.dom", "/test-domains/sbi.smode");
    print_nobuf("SBI domain created with ID %lu\n", dom_id);
    region_id_t region_id = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", region_id);
    char *region_base = map_region(region_id, 4096);
    print_nobuf("Shared region mapped at %p\n", region_base);
    print_nobuf("To share the memory region\n");
    share_region(dom_id, region_id);
    print_nobuf("Shared region shared with domain %lu\n", dom_id);

    print_nobuf("To call domain ID = %lu\n", dom_id);
    print_nobuf("Call returned = %lu\n", call_dom(dom_id));

    capstone_cleanup();

    return 0;
}
