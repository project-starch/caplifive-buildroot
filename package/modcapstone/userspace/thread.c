#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/libcapstone.h"

#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

int main() {
    capstone_init();

    dom_id_t dom_id = create_dom("/test-domains/thread.dom", NULL);
    print_nobuf("Domain created with ID %lu\n", dom_id);
    print_nobuf("To schedule domain\n");
    schedule_dom(dom_id);

    capstone_cleanup();

    return 0;
}
