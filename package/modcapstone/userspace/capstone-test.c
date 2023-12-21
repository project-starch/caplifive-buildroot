#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/libcapstone.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Please provide the domain file name!\n");
        return 1;
    }

    int retval = capstone_init();
    if(retval) {
        fprintf(stderr, "Failed to initialise Capstone\n");
        return retval;
    }

    char* file_name = argv[1];

    unsigned times = 1; // how many times to run the domain
    if (argc >= 3) {
        times = atoi(argv[2]);
    }

    dom_id_t dom_id;
    region_id_t region_id;
    if (argc >= 4) {
        dom_id = create_dom(file_name, argv[3]);
    } else {
        dom_id = create_dom(file_name, NULL);
    }
    printf("Created domain ID = %lu\n", dom_id);

    // region_id = create_region(4096);
    // share_region(dom_id, region_id);

    for (unsigned i = 1; i <= times; i ++) {
        unsigned long dom_retval = call_dom(dom_id);
        printf("Called dom (%u-th time) retval = %lu\n", i, dom_retval);
    }

    retval = capstone_cleanup();
    if(retval) {
        fprintf(stderr, "Failed to clean up Capstone\n");
        return retval;
    }

    return 0;
}
